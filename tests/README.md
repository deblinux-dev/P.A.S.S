# Тесты P.A.S.S.

Полный прогон: `bash tests/run_all.sh [libpass_client.so] [drrun]`
(используется в CI — `.github/workflows/ci.yml`).

## asm-юниты (чистый `_start` + syscall, без libc)

Сравнение нативного исполнения и исполнения под профилем `core2`
(форсированная эмуляция на современном хосте), код выхода 0 = успех:

* `t_pmov2.S` — vpxor/vmovdqu/vinserti128/vpmovmskb (значение + сохранность CF);
* `t_vz.S` — vinserti128 + vzeroupper/vzeroall;
* `t_flags.S` — сохранность EFLAGS вокруг hooked-cpuid; TZCNT/LZCNT значения+CF;
* `t_msk.S` — vpmovmskb ymm с dst=ECX: значение, сохранность rax/rdx/флагов;
* `t_store.S` — 256-битные store/load через память (класс бага v0.1-§8.1);
* `t_str.S` — точная реплика цикла strchrnul_avx2 (класс алиасинг-бага §8.8);
* `t_vq.S` / `t_vq4.S` — vmovq XMM↔GPR в обе стороны (обход краша кодировщика DR).

## дифф-тесты (побайтно native vs pass)

* `t_lztz.S` — матрица LZCNT/TZCNT (11 значений × 3 ширины), хеш
  (результат, EFLAGS) → stdout; обязан совпадать с нативом побайтно;
* `data/ts2.c`, `data/ts3.c` — строковые функции (strlen/strchrnul/strchr/
  memchr/strrchr/strcspn/strstr/strnlen) — хеши и по-членный дамп;
* `data/tp.c` — printf-батарея (A..K: форматы %d/%s/%x/%g/%p/*w.*/…).

## приложения

* `data/h3.c` / `data/h4.c` — memset-only / strlen-only статические;
* `data/hello.c` — «hello static 42» (регрессия стартап-порчи glibc 2.41);
* `data/selftest.c` — KAT-батарея (BMI2/BMI1/LZCNT/TZCNT/POPCNT/CRC32C/
  SSE4.x/AES/PCLMUL/AVX2-память/FMA) против скалярных эталонов на чистом C.
  Одна и та же сборка идёт нативно и под pass (gcc+intrinsics / MSVC+intrinsics).

Ручной прогон одного теста:

```bash
as t_msk.S -o t_msk.o && ld t_msk.o -o t_msk
./t_msk                                 # нативно
drrun -c build/libpass_client.so -target core2 -- ./t_msk
```

Диагностика падений: `-verbose 1` печатает app-PC при SIGSEGV/SIGABRT
(символизация `objdump -d --start-address=…`), `-verbose 3` — все вставленные
последовательности. Методика локализации — docs/ARCHITECTURE.md §6/§8.
