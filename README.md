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
| Динамические бинари + printf-батарея + строковая батарея | ✅ побитно совпадают с нативом |
| Windows: клиент собирается MSVC, trap через exception event, лаунчер pass.exe | ✅ сборка проверяется в CI |
| GitHub Actions: сборка + тесты Linux/Windows, релизы по тегу | ✅ |

### Известные ограничения v0.2

* **selftest под pass изредка падает** — редкий крах, зависящий от раскладки
  кодового кэша (воспроизводится ~1 из N запусков); локализация — приоритет
  v0.3 (см. ARCHITECTURE.md §7). Все остальные тесты стабильны.
* **AVX-512/EVEX/маски (k0–7), SHA-NI, GFNI, RDRAND/RDSEED** — не эмулируются;
  EVEX отсекается явно, остальное ловит trap-фоллбек.
* **FMA** — двойное округление (mul→add) вместо слитного.
* **Fault-семантика gather/maskmov** — маскированные каналы корректно
  пропускаются, но отказ памяти на немаскированном канале внутри clean call
  не транслируется в штатный SIGSEGV приложения.
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
(printf-батарея, строковая батарея, матрица LZCNT/TZCNT с флагами), статические
приложения и KAT-батарея selftest (BMI2/BMI1/AES/PCLMUL/CRC32C/SSE4.x/AVX2/FMA
против скалярных эталонов на чистом C).

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
