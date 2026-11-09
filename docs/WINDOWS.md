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
