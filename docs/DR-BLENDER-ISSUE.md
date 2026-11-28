# DR-BLENDER-ISSUE.md — черновик issue для DynamoRIO (upstream)

Статус: ЧЕРНОВИК. Перед отправкой заполнить все `[FILL]` данными с
машины Penryn (раздел 9 BUN-SPIN-REPORT.md, актуальная редакция).
Файл держим в docs/, чтобы материалы переживали сессии.

---

## Title (предложение)

Windows x64: early-injection fails silently on blender.exe 4.2+ (works on 3.6)

## Body (черновик на английском)

### Environment

* DynamoRIO version: `cronbuild-11.91.20763` (custom build) and
  official `[FILL: точная версия 11.92 debug]`
* OS: Windows 10 `[FILL: билд, 19042/22H2 — как у bun: "win10_fe"]`, x64
* CPU: Intel Core 2 Duo E8400 (Penryn, SSSE3, no SSE4.1 AVX)
* Application: Blender `[FILL: точный билд 4.2.23 — откуда скачан, размер blender.exe, SHA256]`
  Also observed on 4.5.17 (launcher path — to be re-verified directly).
* Control (works fine under the same DR build): Blender 3.6.23 and 2.79b.

### What happens

We run a DynamoRIO client (an instruction-substitution layer for legacy
CPUs). On Blender 4.2.23, DynamoRIO never attaches to blender.exe:

1. `drrun -c <client.dll> -- blender.exe` — the process starts, runs
   NATIVELY (no code cache: the CPU-unsupported dialog appears and the
   process exits). No `[client banner]` is printed; the client's
   `dr_client_main` is never reached (verified by a file-write marker
   at the very first line of `dr_client_main`).
2. Running with no client at all (`drrun -- blender.exe`) reproduces
   the same failure — so this is NOT a client bug.
3. `drrun -debug` (official 11.92 debug build): DR dies during earliest
   init BEFORE emitting a single log line. `[FILL: точный stderr, если
   что-то печатается; где лежит drlog-файл, если создаётся]`
4. `-follow_children` variant (running `blender-launcher.exe`):
   the child dies with `0xC0000142` (STATUS_DLL_INIT_FAILED). `[FILL:
   точная команда и вывод]`
5. Direct launch of blender.exe (bypassing blender-launcher.exe):
   `[FILL: результат — ожидается то же, что и (1)]`

### What we expected

The client to be loaded (as with Blender 3.6.23 / 2.79b on the same
machine, same DR build, same command line).

### Minimal reproduction

```
drrun -c <any client.dll> -- blender-4.2.23.exe
# observe: no client output, app runs natively, CPU-check dialog
```

No client needed:

```
drrun -- blender-4.2.23.exe
```

### Suspected area (from outside)

Blender 4.2+ switched the Windows executable layout significantly
(large `.text`, possibly `[FILL: dumpbin /headers вывод — DLL
CHARACTERISTICS, SectionAlignment, SizeOfImage, наличие
/cetcompat / dynamichighentropyva / largeaddressaware]`). DR's
earliest-injection path dies before any logging, which makes it hard
to bisect from the outside.

Happy to run any instrumented build or provide ETW/Process Monitor
traces on request.

### Extra materials

* Full investigation log (section 9 of our report): `[FILL: приложить
  BUN-SPIN-REPORT.md раздел 9 или процитировать]`
* Works/doesn't-work matrix:

| Blender | DR attaches? |
|---|---|
| 2.79b | yes |
| 3.6.23 | yes |
| 4.2.23 | NO (early-init death / 0xC0000142 with follow) |
| 4.5.17 | suspected NO (launcher: no banner; direct: [FILL]) |
| 5.2.5 | yes (client loads; separate app-level issue) |

---

## Чек-лист данных для заполнения (заполнить на Penryn)

1. `dumpbin /headers blender.exe | head -60` для 4.2.23 и 3.6.23
   (сравнить CHARACTERISTICS / alignment / SizeOfImage).
2. Точная версия официального DR 11.92 (номер релиза/кронбилда).
3. Полный stderr `drrun -debug -- blender.exe` (и путь drlog, если
   создался; `DYNAMORIO_LOGDIR`, `-loglevel 15`, `-logmask 15`).
4. `drrun -follow_children -- blender-launcher.exe` полный вывод.
5. Прямой запуск `blender.exe` 4.5.17 (без лаунчера) под `pass.exe`.
6. Process Monitor trace (только события blender.exe): какие DLL
   грузятся/отказывают до смерти.
7. SHA256 и размер blender.exe каждой версии.

## Гипотезы для проверки ДО отправки (может, найдём сами)

* Размер образа: blender.exe 4.2+ ~[FILL] МБ vs 3.6 ~[FILL] МБ —
  если DR имеет лимит размера образа/числа секций в раннем инжекте.
* `/DYNAMICBASE` + high-entropy VA на 4.2+ при 32-битных зависимостях?
  (нет, x64-only дистрибутив — проверить).
* Различие в TLS-слотах / _tls_used на 4.2+ (Blender перешёл на
  новый тулчейн между 3.6 и 4.2).
* CET/IBT (`/CETCOMPAT`) в заголовке 4.2+ — DR 11.91 не поддерживает
  shadow stack на Windows; если флаг стоит, ядро может включить CET
  и инъекция DR ломается. ПРОВЕРИТЬ ПЕРВЫМ (dumpbin).
