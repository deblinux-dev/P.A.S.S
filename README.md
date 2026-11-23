# P.A.S.S. — Performance Adaptive SIMD Substitution

**v0.2** — инструмент для запуска программ, скомпилированных под новые наборы
инструкций (SSE4.x, AVX/AVX2, FMA, BMI, AES-NI …), на старых процессорах.
Неподдерживаемые инструкции **заменяются** (inline-эмуляция или clean-call),
поддерживаемые исполняются нативно (**passthrough**).

В отличие от Intel SDE, который эмулирует вообще всё и замедляет даже `hello world`
в десятки раз, P.A.S.S. подменяет только то, что не умеет целевой CPU, — оверхед
возникает исключительно на эмулируемых инструкциях.

```
  ╔══════════════════════════════════════════════╗
  ║  P.A.S.S. — Performance Adaptive SIMD        ║
  ║              Substitution                    ║
  ╚══════════════════════════════════════════════╝
```

## Как это работает (кратко)

1. **Статический рескрайнинг** — клиент DynamoRIO перехватывает трансляцию каждого
   базового блока в кодовый кэш. Поддерживаемая целевым CPU инструкция проходит
   как есть; неподдерживаемая заменяется на эквивалент из старых наборов:
   * `VEX.128 → legacy SSE` механически (`vaddps xmm1,xmm2,xmm3` → `movaps/addps`);
   * `VEX.256 → две 128-битные половины`, старшая живёт в **shadow-файле**
     (область клиентского TLS), младшая — в обычных xmm-регистрах;
   * POPCNT, LZCNT/TZCNT, MOVBE, BMI1/2, FMA, broadcast'ы, vpmovmskb и др. —
     специализированные безветвевые последовательности;
   * «сложные» операции (CRC32, AES-NI, PCLMULQDQ, ROUND*, gather'ы, строковые
     сравнения, PEXT/PDEP, LZCNT/TZCNT с точными флагами …) — универсальный
     эмулятор через clean call;
2. **CPUID/XGETBV-маскирование** — программа видит «целевой» процессор плюс
   (опционально) эмулируемые фичи; лист 0xD согласован с подменённым XCR0.
   Диспетчеры glibc/компиляторов сами выбирают пути, которые мы умеем исполнять;
3. **Trap-fallback (гибрид)** — пропущенные инструкции (например, из JIT-кода)
   ловятся по SIGILL (Linux) / EXCEPTION_ILLEGAL_INSTRUCTION (Windows) и
   эмулируются по месту; если эмуляции нет — приложение падает громко, как на
   настоящем железе, с диагностикой в статистике.

Подробно: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
покрытие инструкций: [docs/COVERAGE.md](docs/COVERAGE.md),
запуск на Windows: [docs/WINDOWS.md](docs/WINDOWS.md).

## Статус v0.2 — что проверено

Проверено на DynamoRIO 11.91.20763, Linux x86-64 (хост: Xeon AVX-512;
форсированный профиль `core2` — эмулируется то, что железо умеет нативно,
результат сверяется с нативным исполнением):

| Компонент | Статус |
|---|---|
| CPUID-спуфинг (листья 1, 7, D, 80000001) — согласован с XCR0 | ✅ |
| Механический VEX.128/256 → SSE (ALU/сдвиги/перестановки/moves) | ✅ |
| vmovdqa/vmovdqu/vmovss/vmovsd (все ширины и направления) | ✅ |
| Алиасинг dst==src в VEX.256 (vpcmpeqb ymm2,ymm1,ymm2 …) | ✅ |
| vpbroadcast*, vinsert/vextractf128, vpmovmskb, vzeroupper/vzeroall | ✅ бит-в-бит с нативом |
| Generic-путь: BZHI, PEXT/PDEP, MULX, CRC32C, AES, PCLMULQDQ, строковые SSE4.2 | ✅ |
| LZCNT/TZCNT/POPCNT: значения **и флаги** бит-в-бит с нативом (t_lztz, selftest) | ✅ |
| **Стартап статической glibc 2.41 под полным профилем core2** | ✅ **исправлено** (10 багов, см. ARCHITECTURE §6.12+) |
| FMA-256 (36 форм 132/213/231, ps/pd, mem-операнды) | ✅ эмулируется (двойное округление), сверено с нативом |
| vpextrq/vpinsrq, rip-относительные операнды emu-путей | ✅ |
| Динамические бинари + printf-батарея + строковая батарея | ✅ побитно совпадают с нативом |
| Broadcast-семья целиком (vbroadcastss/sd/f128/i128, vpbroadcastb/w/d/q): inline и generic-пути, алиасинг, VEX.128-обнуление верха | ✅ бит-в-бит с нативом (t_bcast, 18 кейсов × 3 режима) |
| Windows: клиент собирается MSVC, trap через exception event, лаунчер pass.exe | ✅ сборка проверяется в CI |
| **bun 1.4.4 (Windows x64) под `-target core2`: исполнение JS полностью — `bun -e 'console.log(6*7)'` → «42», кириллица, baseline-сборка, файловый режим, `--print`, лестница подсистем (VM-инициализация, арифметика JSC, встроенные объекты, event loop), вся матрица emu_mask (в т.ч. чистый SSE)** | ✅ CI (ARCHITECTURE §7.3, раунды #93–#106) |
| Легаси-формы SSE4.1/4.2/AES/PCLMUL в generic-emu: ptest (0dst_2src), blendv-маска XMM0, неявный источник V=DST у destructive/imm-форм, imm в середине списка операндов [W,imm,V], биты blend/insertps, mpsadbw (8 SAD, скользящее окно), aesdec/aesdeclast (InvSubBytes), aeskeygenassist (X1/X3) | ✅ 9 фиксов (A–I) раунда #106, t_s41 — 49 кейсов бит-в-бит с нативом |
| **Строковые SSE4.2 (pcmpestri/strm, pcmpistri/strm + VEX) — точная семантика SDM §4.1 (Table 4-3/4-4/4-7)**: per-pair форсинг валидности BoolRes, все 4 полярности (masked+ ≡ positive, masked− — XOR только валидных), NUL невалиден (ISTR), wildcard-needle ordered, ZF/SF/CF/OF, выходные маски XMM0 | ✅ раунды #111–#112 — t_istr.S, 230 кейсов × (ECX+EFLAGS+XMM0) бит-в-бит с нативом; #112 закрыл strcspn-регресс glibc из CI #111 |
| **Аудит покрытия VEX-опкодов**: системный класс «не в карте → left-native (мусор тени / SIGILL на Core 2)» закрыт — vpsubusb/w, vpmulhuw, дупы vmovs*dup, полуходы vmovlps/hps/lpd/hpd, cvt-ширины (vcvtdq2pd/vcvtpd2dq/vcvttpd2dq), vpermilps/pd, vmaskmovps/pd (load+store), ~40 VEX-алиасов SSE4.1/4.2/AES | ✅ раунд #111 — t_vx41 (83 кейса, 6 блоков) бит-в-бит с нативом |
| GitHub Actions: сборка + тесты Linux/Windows, релизы по тегу | ✅ |
| **ISA-базовая линия самого клиента (раунд #114)**: `__popcnt64` (MSVC) компилировался прямо в инструкцию POPCNT → `internal crash 0xc000001d` внутри pass_client.dll на реальном CPU без SSE4.2 (натурный репрод: bun_bl.exe, B-2003 — bun исполняет popcnt → trap → emu → падение самого эмулятора). Вся битовая математика emu (popcount/clz/ctz) переведена на SWAR без builtins/intrinsics (gcc при -mno-lzcnt кодирует `__builtin_ctzll` как «rep bsf» = байты TZCNT — тоже вычищено); CI-гвард `scripts/scan_baseline.py` сканирует объектники клиента на мнемоники вне SSE2 и ловит весь класс независимо от toolchain | ✅ гвард зелёный на обеих платформах, poisoned-объект отводится, батарея 27/27 (t_lztz бит-в-бит с нативом) |
| **Флаги trap-пути и семантика VCMPSS/SD (раунд #115)**: trap_emulate вычислял flags_def по жёсткому списку (ptest/pcmpe-формы/adcx/adox) вместо IR-модели — ZF/PF/CF, вычисленные emu для trapped lzcnt/tzcnt/popcnt/bzhi/pdep/pext, не возвращались приложению (ветвление по stale-флагам — тот же механизм порчи, что «malloc(): corrupted top size»; симптом B-2004 на реальном CPU без AVX — паника Zig-аллокатора bun «memory allocation of N bytes failed», невидимая на CI-хостах с AVX, где trap-путь не исполняется). ФИКС: instr_get_eflags → EFLAGS_WRITE_* — как build_desc статического пути. Попутно железом опровергнута гипотеза «vcmpsd определяет ZF/PF/CF»: семья 0F C2 — mask-compare, флаги не пишет (probe: comisd eq → ZF=1, vcmpsd $0/$3/$13 → rflags нетронуты); установка флагов в emu убита до того, как попала в релиз. t_vcmp расширен контролем RFLAGS | ✅ 1536 кейсов × (RFLAGS+DEST) побайтно native vs pass, батарея 27/27 |
| **Профиль `-target penryn` (B-2014)**: Merom + нативный SSE4.1 — легаси-SSE4.1 passthrough без стабов, CPUID честно светит SSE4.1 (profile ∩ host). Для реальных Penryn/Wolfdale CPU и приложений, собранных под SSE4.1 | ✅ Godot 4.5: `--headless --editor --quit` rc=0 за секунды под qemu TCG (раньше под core2 — 1.07 млрд clean call’ов ≈ 8+ мин «зависания») |
| **VEX-SSE4.1 inline (B-2014)**: vpextrX (reg/mem-dst, VEX.256 — выбор половины по imm), vpinsrX (movaps merge + legacy), vpminsb/sd, vpmaxsb/sd, vpackusdw, vpmuldq, vpmulld, vroundps/pd, vptest | ✅ t_pinsv (микро-батарея форм), Godot 4.5 (288М vpextrd ушли из generic в inline) |
| **bun-матрица (Linux, B-2013/2014)**: bun 1.2.14 — builtins/JSON/сортировка/regexp/кириллица ✅ (require — известный баг, см. ограничения); bun 1.3.14 **baseline** — полностью ✅ включая require+кириллицу; bun 1.4.5 — полностью ✅ включая require+кириллицу (рекомендация для пользователей) | ✅ qemu Penryn + клиент core2/penryn |
| **Windows B-2015**: глотание системных исключений 0x406D1388/0x40010006/0x4001000A (thread-name/OutputDebugString — их поднимают драйверы NVIDIA/AMD/Intel и Vulkan-слой при инициализации графики; под DR <=v0.2.4 уводили процесс в unhandled → Godot 4.5 «окно появилось и закрылось», ExitStatus 0x406D1388) + исправлена семантика exception-пути (dr_redirect_execution) + [SC]-трейс гейтится -strace (фильтр syscall'ов мёртв в DR 11.91.20763 — 0 call sites в core) + modmap.txt только под -verbose 2 | ✅ tests/data/tname.c в CI (MSVC — обязательный, MinGW — диагностический); Godot 4.5 у пользователя доходит до окна и далее |

### Известные ограничения v0.2

* **AVX-512/EVEX/маски (k0–7), SHA-NI, GFNI, RDRAND/RDSEED** — не эмулируются;
  EVEX отсекается явно, остальное ловит trap-фоллбек (громкая остановка
  с именем опкода — ловушка UNENCODABLE и гард «тихого пропуска» §7.3).
* **FMA** — двойное округление (mul→add) вместо слитного.
* **Fault-семантика gather/maskmov** — маскированные каналы корректно
  пропускаются, но отказ памяти на немаскированном канале внутри clean call
  не транслируется в штатный SIGSEGV приложения.
* **Linux + DR без клиента** — bun виснет на чистом DR (несовместимость DR 11.91 и bun на Linux, §7.2); под клиентом bun 1.2.14/1.3.14-baseline/1.4.5 исполняются (close_range-обход B-2013).
* **bun 1.2.14 `require('fs')`** и **bun 1.3.14 (AVX2-сборка)** — детерминированная порча в стартап-путях (SIGSEGV/panic, класс B-2015: контекстозависимый дефект спец-танца/generic-пути). Воркараунды: bun **1.4.5** или baseline-сборки 1.2.14/1.3.14.
* **Windows + `bun -e` (любая сборка)** — до B-2015 процесс висел на выходе: фильтр syscall'ов мёртв в DR 11.91.20763 (instrument_filter_syscall не вызывается core'ом — 0 call sites), каждый syscall гонял clean call с dr_fprintf; при завершении потоки убивались внутри clean call'ов с DR-локами → NtTerminateProcess не завершал процесс (CI §7.3: kill→exit(143) не умирал до -k). В B-2015 трейс гейтится самим клиентом — окно дедлока устранено; требуется перепроверка на реальном JS в CI.
* **Windows + MinGW-клиент** — selftest ловит internal crash в DR (MSVC-клиент — зелёный); расследуется.
* **Диагностические гейты на JS** (не продуктовый путь): `-nospecial` —
  hang после парсинга; `-nomsk` — 256-битный vpmovmskb остаётся left-native
  и читает верх реального ymm (мусор инварианта тени); `-expose_emulated 0`
  (честный Core 2 CPUID) — bun сам abort (требует SSE4.2 в CPUID, это
  чек стартапа, не инструкция).
* **F16C** (vcvtps2ph/vcvtph2ps) — VEX-опкоды, emu-реализация не готова
  (округление half-float по MXCSR) — следующий раунд.
* **Системные требования** — DynamoRIO 11.x, 64-бит.

## Сборка

Вручную:

```bash
# 1. DynamoRIO 11.x
curl -LO https://github.com/DynamoRIO/dynamorio/releases/download/cronbuild-11.91.20763/DynamoRIO-Linux-11.91.20763.tar.gz
tar xzf DynamoRIO-Linux-11.91.20763.tar.gz

# 2. P.A.S.S.
cd pass && mkdir build && cd build
cmake .. -DDynamoRIO_DIR=/path/to/DynamoRIO-Linux-11.91.20763/cmake
make -j
# → build/libpass_client.so
```

Требования: cmake 3.7+ (**не 4.x** — bundled DrMemory-конфиг DR несовместим,
в репозитории есть обход), gcc/clang (Linux) или MSVC 2019+ (Windows), x86-64.

**Автоматически** — push в репозиторий собирает Linux и Windows артефакты
(`.github/workflows/ci.yml`); тег `v*` создаёт GitHub Release с готовыми
архивами (`PASS-linux-x64.zip`, `PASS-win-x64.zip`).

## Запуск

### Linux

```bash
drrun -c libpass_client.so [опции] -- программа [аргументы]
# или обёртка из релизного пакета:
./pass -target core2 -- ./my_avx2_app
```

### Windows — просто и понятно

```bat
pass.exe -target core2 -- my_avx2_app.exe args...
```

`pass.exe` сам находит (а при отсутствии — скачивает) DynamoRIO и запускает
программу под клиентом. Подробности: [docs/WINDOWS.md](docs/WINDOWS.md).

Опции клиента:

| Опция | По умолчанию | Описание |
|---|---|---|
| `-target <имя>` | `core2` | Профиль целевого CPU: `core2`, `nehalem`, `westmere`, `sandybridge`, `haswell`, `native` |
| `-expose_emulated 0/1` | 1 | Светить эмулируемые фичи в CPUID |
| `-inline 0/1` | 1 | Inline-эмуляция (быстрый путь) |
| `-generic 0/1` | 1 | Универсальный clean-call эмулятор |
| `-trap 0/1` | 1 | SIGILL/exception-fallback |
| `-verbose 0..4` | 0 | 1 — диагностика падений (SIGSEGV/SIGABRT с app-PC), 3 — дамп последовательностей, 4 — трассировка значений |
| `-nospoof` | выкл | Отключить CPUID/XGETBV-витрину |
| `-emu_mask <hex>` | — | Переопределить набор эмулируемых фич (бит k = feat_t k), для бисекции |
| `-no256` / `-no128` / `-nospecial` / `-nobcast` / `-nomsk` / `-novz` | выкл | Диагностические рубильники для бисекции путей эмуляции |

По завершении процесса печатается сводка: число переписанных/эмулированных
инструкций по опкодам, срабатываний trap.

## Тесты

`bash tests/run_all.sh` — полный регрессионный прогон (используется в CI):
asm-юнит-тесты на чистом `_start`+syscall, дифф-тесты «native vs pass побайтно»
(printf-батарея, строковая батарея, матрица LZCNT/TZCNT с флагами, строковые
SSE4.2 t_istr — 230 кейсов ECX+EFLAGS+XMM0, VEX-алиасы t_vx41 — cvt/permil/
maskmov, скалярные сравнения t_vcmp — 1536 кейсов: 32 предиката VCMPSS/SD ×
reg/mem × double/float), статические приложения и KAT-батарея selftest
(BMI2/BMI1/AES/PCLMUL/CRC32C/SSE4.x/AVX2/FMA против скалярных эталонов на
чистом C).

## Структура проекта

```
pass/
├── CMakeLists.txt
├── src/
│   ├── pass.c            — точка входа, опции, TLS, статистика
│   ├── pass.h            — общие структуры, TLS/BUF-макет, портируемость
│   ├── features.c        — хост/профили, опкод→фича
│   ├── cpuid_hook.c      — маскирование CPUID/XGETBV, лист 0xD
│   ├── rewrite.c         — диспетчер BB-прохода, generic-эмиттер
│   ├── rewrite_vex.c     — механический VEX→SSE (128/256+shadow, алиасинг)
│   ├── rewrite_special.c — BMI/FMA/broadcast/pmovmskb/vzeroupper/movbe…
│   ├── emu.c             — универсальный clean-call эмулятор (+LZCNT/TZCNT)
│   └── trap.c            — SIGILL (Linux) / exception (Windows) fallback
├── tools/pass_launcher.c — pass.exe: простой запуск на Windows
├── tests/                — asm-юниты, дифф-батареи, selftest, run_all.sh
├── .github/workflows/    — CI (Linux+Windows) и релизы
└── docs/                 — ARCHITECTURE.md, COVERAGE.md, WINDOWS.md
```

## Дорожная карта

* v0.3: локализация редкого layout-зависимого краха selftest (§7.1);
  точный FMA-режим (слияние через чистый x87-путь в emu.c);
* v0.4: кэширование трансляций, интеграция `drx_expand_scatter_gather`
  для fault-точной эмуляции gather'ов;
* исследовательское: гибрид с trap-and-patch для JIT-кода (Bun/JavaScriptCore).

## Лицензия

MIT. DynamoRIO — BSD.
