# P.A.S.S. на Windows

## Быстрый старт (просто и понятно)

```bat
pass.exe -target core2 -- my_avx2_app.exe args...
```

Всё. `pass.exe` — лаунчер, который:

1. находит **полную** установку DynamoRIO (bin64\drrun.exe +
   lib64\release\dynamorio.dll — «огрызок» из одного drrun.exe не
   принимается: без lib64 DR не может инжектиться и пишет
   «incomplete installation») в каталоге pass.exe (включая
   подпапки dynamorio\ и DynamoRIO-Windows-*\), в
   %DYNAMORIO_HOME% или %LOCALAPPDATA%\PASS\dynamorio;
2. если DynamoRIO не установлен — **скачивает релиз автоматически**
   (~450 МБ, один раз, через PowerShell: TLS 1.2 включён, распаковка
   через tar или Expand-Archive);
3. запускает программу под клиентом pass_client.dll, передавая drrun
   ЯВНЫЙ ключ `-root` (без него drrun, запущенный вне bin64\, ищет
   корень на два уровня вверх — источник ошибки «cannot find
   …\lib64\drpreinject.dll» v0.2.0);
4. передаёт её код возврата в консоль (`%ERRORLEVEL%` работает).

Ключи лаунчера:

```bat
pass.exe -h                       справка (по-русски)
pass.exe -target haswell -- app.exe
pass.exe -root C:\DR\DynamoRIO-Windows-11.91.20763 -- app.exe
pass.exe -verbose 2 -- app.exe    диагностика
pass.exe -no-download -- app.exe  запрет авто-скачивания
```

Ручная установка без скачивания: распакуйте релиз DynamoRIO
(папку DynamoRIO-Windows-11.91.20763 ЦЕЛИКОМ, вместе с lib64\)
рядом с pass.exe — лаунчер найдёт её по маске DynamoRIO-*.

Всё, что стоит до `--`, уходит клиенту P.A.S.S. (те же ключи, что и на
Linux: `-target`, `-verbose`, `-nospoof`, `-emu_mask`, …), всё после `--` —
приложению. Отладка командной строки: переменная `PASS_LAUNCHER_DEBUG=1`.

## Что входит в релизный пакет

```
PASS-win-x64.zip
├── pass.exe           ← лаунчер (статический, MSVC, без зависимостей)
├── pass_client.dll    ← клиент DynamoRIO (P.A.S.S.)
├── README.md, docs\   ← документация
└── LICENSE
```

DynamoRIO докачивается при первом запуске (или положите распакованный
релиз DynamoRIO рядом: `drrun.exe` рядом с `pass.exe`).

## Сборка из исходников

Требуется MSVC 2019+ (или MinGW-w64):

```bat
cmake -S . -B build -DDynamoRIO_DIR=C:\path\to\DynamoRIO-Windows-11.91.20763\cmake
cmake --build build --config Release

cl /O2 /Fe:pass.exe tools\pass_launcher.c
```

## Что проверяется в CI

Windows-job в `.github/workflows/ci.yml` на каждом push:

1. клиент `pass_client.dll` собирается MSVC (портируемость кода: атомики,
   cpuid, арифметика — без GCC-специфики);
2. `pass.exe` собирается и присутствует в пакете;
3. `selftest.exe` (та же KAT-батарея, что и на Linux) исполняется под
   `drrun -c pass_client.dll -target core2` — проверка CPUID-витрины,
   переписывания и эмуляции в Windows-режиме.

## Отличия Windows-режима (v0.2)

* **Trap-fallback** — эмуляция по месту через `drmgr_register_exception_event`
  (EXCEPTION_ILLEGAL_INSTRUCTION = 0xC000001D): контекст правится по месту,
  исключение не доставляется приложению. Полный аналог Linux-SIGILL-пути.
* **RAW TLS** — клиентский TLS через `dr_raw_tls_calloc` работает одинаково
  на обеих ОС (на Windows слоты выделяются DR в TEB); инвариант shadow-файла
  тот же. Формула доступа C-кода: `*(uint64_t*)(dr_get_dr_segment_base(tls_seg)
  + tls_raw_off + off)` — согласована с `gs:[disp]` в сгенерированном коде.
* **CPUID-витрина** — идентична Linux (тот же код, `__cpuid`/`__cpuidex`).

## Известные ограничения (Windows)

* Пакет тестируется в CI smoke-тестом (selftest KAT под
  `drrun -target core2`). Статус v0.2: клиент собирается MSVC,
  лаунчер работает, но KAT-батарея на Windows **не зелёная** —
  воспроизводятся: (1) неверные результаты `mulx` (MULX-путь в
  MSVC-сборке), (2) крах клиента (0xC0000005) дальше по батарее.
  Оба задокументированы в ARCHITECTURE.md §7 (приоритет v0.3);
  лог CI: шаг «Smoke — selftest under drrun -target core2».
* Прогоны на реальном старом железе (настоящий Core 2 Duo)
  приветствуются — ожидается, что HW без AVX даст те же результаты
  через passthrough + эмуляцию.
* JIT-процессы (Bun/Node) на Windows не проверялись. bun на Linux
  виснет даже под чистым DynamoRIO 11.91.20763 без клиента
  (см. ARCHITECTURE.md §7.2) — несовместимость DR×bun, не P.A.S.S.;
  практический обход для bun — Intel SDE (`sde -future -- bun.exe ...`).

## B-2016 (v0.2.6): разбор натурных дампов bun 1.4.5 + диагностика смерти процесса

Натурный материал пользователя (Core 2 Duo E8400, Win10 19042, v0.2.5):
`bun_strace.log` (-strace), 8 минидампов WER (`bun_baseline.exe*.dmp`),
`exc.log` (-verbose 2). Разбор парсером минидампов (scripts/dump_parse2.py)
дал два РАЗНЫХ сценария смерти в одном и том же PID:

1. **`memory allocation of 5085/5184 bytes failed`** — Rust
   `handle_alloc_error` → `abort()` → `__fastfail(7)` → `0xC0000409`.
   fastfail ОБХОДИТ все user-mode обработчики: DR exception event не
   стреляет, итоги не печатаются. Стек (разбор .pdata):
   crash-handler-init → глобальный-аллокатор-обвязка → mimalloc
   realloc → page-map lookup → NULL → паника. Размеры 5085/5184
   соответствуют объёму env-блока пользователя; Windows-путь
   `convert_env_to_wtf8()` (UTF-16→UTF-8, simdutf) на Linux не
   выполняется вовсе — потому qemu-Linux-прогоны этот код не покрывают.
2. **`0xC000001D` (#UD) на `popcnt %r15,%r12`** (bun_baseline+0x25ec)
   при dynamorio.dll **в процессе, но pass_client.dll НЕ загружен**
   (модуль-лист дампа) — приложение исполнялось «голым»: без спуфа
   CPUID и без перезаписи POPCNT инструкция ушла в реальный Penryn.
   Контекст: R14=база образа, R12=base+0x1000, R15=0x1000 — скан PE.

### Что добавлено (B-2016)

* **Кольцевой буфер последних 32 generic-опов** (emu.c) — пишется на
  каждом generic-вызове, печатается в итогах, при diag-исключениях и
  при OOM-детекте. Это единственная возможность узнать, ЧТО
  исполнялось перед fastfail-смертью.
* **OOM-детектор в pre-syscall** (strace.c, Windows, без -strace):
  NtWriteFile(54)/NtDeviceIoControlFile(7) — скан буферов записи на
  «memory allocation of»; при обнаружении — полный дамп состояния
  (кольцо + счётчики + регистры). Fastfail после этой печати уже не
  догнать, но дамп успевает выйти в консоль.
* **Дамп при diag-исключениях** (trap.c): #UD/stack-overflow — печать
  кольца + счётчиков + регистров app-контекста до эмуляции/доставки.
* **Лаунчер**: при exit-коде 0xC000001D печатает подсказку «эмулятор
  не был активен в момент #UD» (баннер клиента отсутствовал = клиент
  не загрузился; проверить полную установку DR).

### Гипотеза о причине OOM (не закрыта — требует натурного дампа с B-2016)

Вероятностная цепочка: emu-оп (в окне фейла: vpmaxuw ×2, pinsrd ×7 —
ровно тот набор, которым simdutf валидирует/конвертирует UTF-16) даёт
искажённый результат → неверная длина/содержимое WTF-8 буфера → порча
метаданных mimalloc → следующий realloc → NULL → паника. Прямого
доказательства нет: qemu-Linux прогон bun 1.4.5 с богатым env (90
переменных, кириллица) под -target penryn проходит (32/32 тестов
сюита — зелёная), т.к. UTF-16-конвертация на Linux не исполняется.
Дамп B-2016 (кольцо опов прямо перед OOM) локализует виновника на
натурном железе в первом же прогоне.

## B-2017 (v0.2.7): детектор OOM починен — карта параметров NT-вызовов

### Ключевой дефект B-2016 (найден по натуральному -strace-логу пользователя)

OOM-детектор B-2016 **не срабатывал никогда**: у NtDeviceIoControlFile
читались param5/param7 как «буфер/длина» — на деле это IoControlCode и
InputBufferLength; dr_safe_read уходил по адресу, равному коду IOCTL,
и всегда отбивался. Правильная карта: inbuf=param6, insize=param7,
outbuf=param8, outsize=param9 (у NtWriteFile буфер действительно
param5/param6 — путаница 0-based/порядок аргументов). У пользователя
v0.2.6 дамп `[P.A.S.S.][OOM]` так и не появился — детектор молчал при
живом сообщении в condrv-записи.

### Что добавлено (B-2017)

* **Карта параметров исправлена** (strace.c): SC 54 → buf=param5/len=param6;
  SC 7 → inbuf=param6/insize=param7 + outbuf=param8/outsize=param9.
* **Скан UTF-16LE-паттерна** «memory allocation» — на случай WriteConsoleW-
  пути консольной записи (condrv передаёт буфер как UTF-16).
* **Парсинг размера** из сообщения: печатается запрошенный mimalloc'ом
  объём (у пользователя детерминированный 5085 ≈ env-блок в UTF-8).
* **Кольцо опов получило app-PC**: каждая запись печатается как
  `op#N@имя_модуля+0xRVA` — по дампу сразу видно, ЧЬЯ это инструкция
  (simdutf? mimalloc? JSC? CRT?), без ручной привязки адресов.

### Результаты офлайн-репродукции (B-2017, песочница Linux)

Все проверки — под `-target penryn` на AVX2-хосте (spoof → haswell-ядра,
эмулируются клиентом):

* simdutf 7.4.0 (gcc-сборка): `utf8_length_from_utf16le` +
  `convert_utf16le_to_utf8_with_errors` на env-подобном буфере
  (60 переменных, кириллица/emoji/иероглифы) — длина, содержимое и
  отсутствие записи за буфер совпадают с нативным побайтно;
* bun-linux-x64 1.4.5: `console.log` кириллицы и `TextEncoder.encode`
  + `encodeInto` (10.4k generic-эмуляций: vptest/vpminuw/vpmaxuw/
  vpblendvb/…) — выход бит-в-бит с нативным;
* дизассемблерный скан официального bun-windows-x64.exe (v1.4.5):
  все 256-битные VEX-опкоды, которые bun реально исполняет на
  AVX2-пути, лежат внутри таблицы покрытия P.A.S.S. (AVX-512/VAES/
  VPCLMULQDQ-256/vpdpbusd не диспетчеризуются без спуфа этих фич).

Вывод: simdutf-ядро конвертации env **не** воспроизводит порчу на
Linux-стеке — виновник в Windows-only пути (crash-handler init /
mimalloc-форк / PEB-перестройка env). Натурный дамп B-2017 с кольцом
опов+PC на реальном E8400 — следующий решающий шаг.

