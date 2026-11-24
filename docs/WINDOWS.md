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

## B-2019 (v0.2.9): разбор стека паники B-2018 — паника РУСТОВСКАЯ, аллокатор — собственный bun, отказ на bitmap-учётке страниц

### Символизация офлайн (без PDB)

Модуль в логе B-2018 называется `bun-profile.exe`, потому что **стандартная
сборка bun.exe 1.4.5 экспортирует внутреннее имя `bun-profile.exe`** (проверено
pefile: export-name «bun-profile.exe», SizeOfImage 0x6fef000, TimeDateStamp
0x6afea993). PDB к стандартной сборке не публикуется — символизация выполнена
по самому бинарю: границы всех 98 694 функций взяты из `.pdata`, дизасм
(capstone) + строковые xref (.rdata/.data) + call-xref (E8 rel32). Утилиты
разбора: scripts/analyze_bun.py, call_xref.py, func_disasm.py, find_oom_xref.py,
dump_location.py (работают по любому RVA из будущих логов).

### Что показал стек паники (лог B-2018, «43» так и не напечатано)

* Часть «кандидатов» — ДАННЫЕ, не код: `0x668cc09` = сама строка
  **«memory allocation of \xc0\x0e bytes failed\n»** (куски формата Rust:
  «memory allocation of » + « bytes failed\n»), рядом
  «skipping backtrace printing to avoid potential recursion» — это
  **`std::panicking` Rust**, а не Zig-паника (нет префикса «thread N panic:»);
  запись в 3 чанка = Rust format_args пишет куски по отдельности.
* `0x6acbe40/0x6acc7c0/0x6accce0` (.data) — тройки 32-битных RVA = unwind-таблицы
  (.bunblt-секции), которые заталкивает на стек захват backtrace при панике.
* Внутренние кадры печати: `0x386c750` = writer Rust std на **NtWriteFile**
  (проверка `cmp eax,0x103` = STATUS_PENDING → WaitForSingleObject(INFINITE) →
  RtlNtStatusToDosError — IAT подтверждён), `0x387aa50` = console-ветка
  (GetConsoleMode/GetConsoleOutputCP==65001). Ровно SC 8, который ловит детектор.
* Цепь аборта (все звенья найдены call-xref'ом): vec-grow `0x5298b00` →
  oom-диспетчер `0x5298d18` → `0x5298cff` (swap) → `0x386b4e0` (swap) →
  `0x3875540` handle_alloc_error → `0x386b0b0`: `call 0x387549d; mov ecx,7; int 0x29`
  (**fastfail FAST_FAIL_FATAL_APP_EXIT**). Статические panic-Location
  (`0x28ff070`) и таблицы кусков (`0x668cc08/0x668cc2f`) — точно те значения,
  что висели на стеке (+0x02d0, +0x0140).
* **Паникующий код — поток конвертации окружения** `0x1cc4f70`
  (`convertEnvToWTF8`, строка-ассерт «convertEnvToWTF8 may only be called once»
  в 0x63a7cf6; Location-структуры Rust в сборке bun заглушены «<redacted>»):
  обход env-блока (UTF-16, `call 0x51e1dc0` = длина строки, цикл до
  двойного нуля) → `call 0x5ba0(env, total_len)` → vec-grow `0x5298ba0`
  (hashbrown-математика: `mul` + `seto` + порог 0x8000000000000000) →
  `call 0xc440` (realloc TLS-heap: быстрый путь ≤0x400 — freelist по
  выровненному размеру, `add [page+0x10], 0x10001` = счётчики used/cap) →
  медленный путь `call 0x5f10` (size-class через BSR/XOR 0x3F; порог
  0x27FF) → grow-page `0x6250` (housekeeping каждые 1000 аллокаций —
  счётчик +0x530 ≥ 0x3E8) → класс-страница `0x120dd70` / page-get `0x6830`
  → page-make `0x120db40` → seg-get `0x72a0` → bitmap find `0x89c0` →
  **claim `0x8810`: `lock cmpxchg` CAS-цикл + `popcnt` + `sete dil`**
  (успех claim = «ни один бит маски не был занят» = popcnt==0 → ZF) и
  **scan `0x9100`: `tzcnt`** по маскам `[seg+idx*8+0x40/0x80/0xC0]`.
* **NULL-путь найден и он был на стеке**: `0x6624 test rax; jne 0x6817` —
  после page-refill NULL → `lea rdx,[0x68d6890]` = строка
  **«unable to allocate memory (%zu bytes)\n»** → `call 0x11f9780` (report,
  в релизе молчит) → **`0x66e8: xor eax,eax` → NULL** — адрес 0x66e8
  присутствует в STACK-дампе B-2018 (+0x0330). Второй NULL-путь
  (отрицательный size → «allocation request is too large») для 5085
  исключён: размер положительный.
* Значит: **все NT-вызовы SUCCESS, а отказ — ВНУТРИ учётки страниц
  аллокатора**, ровно там, где P.A.S.S. эмулирует POPCNT/TZCNT
  (op#539/op#1059 в кольце B-2018 — 17 исполнений claim/scan прямо
  перед смертью). Это главный подозреваемый, но не приговор: та же
  симптоматика даёт и рассинхрон commit-учётки, и провал в слепой
  зоне -strace (NtCreateSection/NtMapViewOfSection не трассировались).

### Расшифровка op# (dr_ir_opcodes_x86.h DR 11.91.20763)

`op#844 = vpmaxuw` (319 — лидер; UTF-конвертация simdutf-класса),
`op#840 = vpminuw`, `op#539 = popcnt`, `op#1059 = tzcnt`,
`op#739 = vcmpsd`, `op#686 = vaddsd`, `op#859 = vroundsd`,
`op#825 = vpmovsxwq`, `op#835 = vpmovzxwq`, `op#764 = vcvtdq2pd`.
Popcnt/tzcnt идут статическим generic-путём (rewrite.c generic_opcs;
флаги восстанавливаются безусловно (6)-рестором, t_lztz покрывает) —
битая 16/32-битная ветка try_popcnt в rewrite_special.c в диспетчеризацию
не попадает (мёртвый код; помечена на вычищение в v0.3).

### Что добавлено (B-2019)

* **probe-alloc: 9 drwrap-зондов по RVA стандартной сборки 1.4.5**
  (гейт: SizeOfImage == 0x6fef000, иначе пропуск с печатью size/ts —
  пришлёшь лог, соберу таблицу под другую сборку тем же .pdata-методом):
  `report` 0x11f9780 (pre: печать fmt-строки и аргумента — УВИДИМ текст
  «unable to allocate memory (%zu bytes)» и размер), `grow` 0x6250,
  `classpg` 0x120dd70, `pageget` 0x6830, `pagemake` 0x120db40,
  `segget` 0x72a0, `findpg` 0x89c0, `claim` 0x8810, `scan` 0x9100
  (post: печать NULL-возвратов + дамп heap-полей +0x410/+0x430/+0x438/
  +0x440/+0x530/+0x534/+0x538). Авто — вместе с -strace; -probealloc /
  -noprobealloc — форс/гейт.
* **Кольцо generic-опов стало per-thread** (TLS): глобальное кольцо
  B-2016/2018 перемешивало нити — в логе B-2018 «последние опы перед
  паникой» содержали запросы watcher-потока ([SC 7] NtDeviceIoControlFile
  вклинивались в печать паники). OOM-дамп печатает кольцо ПАНИКУЮЩЕЙ
  нити; побочно починен exit-дамп (кольцо освобождалось thread-exit'ом
  до exit-события — DR-хип затирал первые 8 байт, UAF-чтение).
* **-strace: SC 118 перекартирован** (Win10 19041+/Win11 сигнатура БЕЗ
  ZeroBits: (P, *Base, *RegionSize, Type, Protect, …) — лог B-2018
  показывал r9-указатель на RegionSize, а «type=0x4» был защитой
  PAGE_READWRITE) и добавлен **`[SC N retbase]`** — фактическая база
  результата SC 24/118/30/80 (куда встал 1GB-резерв с base=0 раньше
  было не видно).
* claim/scan-флаги ещё раз сверены с SDM (popcnt: ZF=рез==0; tzcnt:
  CF=src==0; consumers в бинаре — только sete после popcnt и cmovb по
  собственному sub) — эмулятор в норме, вопрос за натурными зондами.

### Ожидание от натурного прогона (bun 1.4.5, -target penryn -strace)

Перед паникой появится блок `[P.A.S.S.][PROBE]`:
`report(a0=… fmt="unable to allocate memory (%zu bytes)" a=5085)` и
строки `segget/pageget/pagemake/claim -> NULL (вызов #N, отказ #M)` с
дампом heap. Читаем так: **первый NULL в цепочке снизу вверх — и есть
точка отказа**; если NULLит только `claim` при SUCCESS-коммитах рядом —
это наша эмуляция POPCNT/TZCNT/флагов в bitmap-учётке (фикс в v0.3);
если NULLит `segget` при пустом claim-логе — учётка резерва/коммита
(смотрим [SC 118 retbase] рядом); если `report` печатает «allocation
request is too large» — переполнение размера (порча данных выше).


## B-2018 (v0.2.8): детектор OOM наконец ловит панику — номер NtWriteFile был «призраком»

### Почему [OOM]-дамп молчал в B-2016 И B-2017

Натурный -strace-лог пользователя (bun 1.4.5, v0.2.7) закрыл вопрос. По
j00ru/windows-syscalls (Win10 20H2 x64, совпадает с 19042 пользователя):

* **NtWriteFile = SC 8 (0x08)** — а детектор сканировал **SC 54**!
* SC 54 (0x36) = **NtQuerySystemInformation** — у него param5/6 вообще
  не буфер записи (детектор читал мусор и отбивался на dr_safe_read).
* В логе пользователя куски паники перемешаны ровно с `[SC 8] 0x58 …`
  (NtWriteFile на консольный хэндл): `memory allocation of` → `[SC 8]`,
  `5085` → `[SC 8]`, `bytes failed`. Номер-призрак «NtWriteFile(54)»
  кочевал по комментариям кода с B-2016.

Второй баг той же линии (найден аудитом): ASCII-паттерн детектора
проверял `'a','l','l'` в смещениях +16..18 — но в строке
«memory allocation of …» там стоят `'n',' ','o'` («allocatio|n …»),
т.е. ASCII-ветка не могла сработать НИКОГДА. UTF-16-ветка была верной,
но без SC 8 бесполезной.

### Что добавлено (B-2018)

* **OOM-детектор сканирует SC 8 (NtWriteFile: buf=param5/len=param6)**
  + SC 7 (condrv-ioctl, карта B-2017), окно 512Б (текст в condrv-сообщении
  может лежать за ~240-байтным заголовком), оба паттерна (ASCII
  исправлен, UTF-16LE сохранён).
* **Стек-скан в момент паники** (`pass_diag_stack_scan`): 16КБ от RSP,
  до 40 кандидатов `module+RVA` — bun-фреймы символизируются офлайн
  по .pdata (как в разборе дампов B-2016).
* **Дамп на NtTerminateProcess(self)** (SC 44): ExitStatus + стек-скан +
  кольцо опов + регистры — второе гарантированное окно (Zig exit-путь
  через ExitProcess доходит до syscall; fastfail — нет).
* **Расшифровка аллокационной семьи в -strace**: дерефы
  `*RegionSize/*BaseAddress` у SC 24/118/30/80 — в логе видно, ЧТО
  резервирует mimalloc (сегменты? коммиты page-map?) — ключ к
  «page-map lookup → NULL» из разбора дампов B-2016.
* **Зонд IsProcessorFeaturePresent** (kernel32+kernelbase, drwrap,
  под -strace): каждый запрос `(код → нативный ответ)`. Функция читает
  KUSER_SHARED_DATA — заполнен ядром по НАСТОЯЩЕМУ CPU, спуф CPUID
  сюда не доходит. Кандидат на «CPU lacks AVX support» у baseline-bun
  (CPUID-спуф P.A.S.S. согласован: OSXSAVE/XSAVE/leaf-D выставляются
  вместе с AVX, XCR0=0x7 — значит запрос шёл мимо CPUID).
* Офлайн-юнит детектора: scripts/test_oom_pattern.c — 7/7
  (ASCII-чанк, UTF-16LE, текст на смещении 240, анти-тесты, и
  контрольный тост: старый ASCII-паттерн действительно мёртв).

### Ожидание от натурного прогона (bun 1.4.5, -target penryn -strace)

При панике появится блок `[P.A.S.S.][OOM]` (SC 8, ASCII):
запись-чанк, запрошенный размер, `[STACK]`-кандидаты module+RVA
(кадры от точки смерти до входа в панику), кольцо последних 32
generic-опов `op#N@модуль+RVA`, top-счётчики, регистры. По нему
определяем: (а) чей код был последним (simdutf env-конвертации?
crash-handler?), (б) какие эмулированные опы шли вплотную к фейлу
mimalloc — следующая итерация фиксации делается ПО ЭТИМ ДАННЫМ.
