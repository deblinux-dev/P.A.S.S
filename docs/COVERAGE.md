# Покрытие инструкций P.A.S.S. v0.1

Легенда: **inline** — безветвевая последовательность в кодовом кэше (быстро);
**generic** — clean-call C-эмулятор (~100 нс); **trap** — не эмулируется,
ловится SIGILL-фоллбеком с диагностикой; **native** — passthrough.

## AVX/AVX2 (VEX)

| Группа | Опкоды | Способ |
|---|---|---|
| Арифметика FP/INT 128/256 | vadd/vsub/vmul/vdiv/vmin/vmax ps,pd,ss,sd; vpadd/vpsub/vpadds/vpaddus/vpsubs/vpavg; vpand/vpor/vpxor/vpandn | inline (механика NDS/ND2) |
| Перестановки/упаковки | vpunpck*, vpacksswb/dw, vpackuswb, vpshufb, vpalignr, vpsign*, vpabs*, vphadd*, vphsub*, vpmul*, vpmadd*, vpsadbw, vpblendw, vshuf*, vcmpp*, vunpck*, vhadd*, vhsub*, vaddsub* | inline |
| Сдвиги | vpsllw/d/q, vpsrlw/d/q, vpsraw/d, vpslldq, vpsrldq (imm и xmm-счётчик) | inline (VF_SHIFT) |
| «Чистые» | vsqrtp*, vrcpps, vrsqrtp*, vcvtdq2ps, vcvtps2dq, vcvttps2dq, vpshufd, vpshuflw/hw, vcomis*, vucomis*, vcvts*s*→si | inline (VF_ND2) |
| Перемещения | vmovaps/upd/apd/upd, vmovd/q, vmovss/sd (2- и 3-оп.), vmovntp*, vlddqu, vld/stmxcsr | inline (VF_DIR; 256 — split) |
| Сравнения с imm | vcmpps/pd/ss/sd | inline |
| Конверсии | vcvtsi2ss/sd, vcvtss2sd, vcvtsd2ss | inline; vcvtps2pd (ymm←xmm), vcvtpd2ps | generic |
| Broadcast | vbroadcastss/sd/f128/i128, vpbroadcastb/w/d/q (reg и mem) | inline |
| Комбинирование половин | vinsertf/i128, vextractf/i128 | inline |
| Маскирование бит | vpmovmskb, vmovmskps/pd (128 и 256) | inline (lo\|hi<<n через shadow, флаги сохраняются) |
| Состояние | vzeroupper, vzeroall | inline (64 mov-imm; +pxor-цепочка) |
| Переменные сдвиги | vpsllv*, vpsrlv*, vpsrav* | generic |
| Перестановки cross-lane | vpermd, vpermq, vperm2f128, vperm2i128 | generic |
| Смешение | vpblendd, vblendps/pd (imm), blendv*, pblendvb, vtestps/pd | generic |
| ROUND/DOT | vroundps/pd/ss/sd, vdpps/dppd, mpsadbw, phminposuw, insertps | generic |
| maskmov/gather | vpmaskmovd/q (load/store), vgatherdd/dq/qd/qq, vgatherdps/dpd/qps/qpd | generic (маскированные каналы пропускаются) |
| min/max SSE4.1-набор | pminsb/sd/uw/ud, pmaxsb/sd/uw/ud | generic |
| Расширения | pmovsx*, pmovzx* (12 вариантов), packusdw, pmulld, pmuldq, pcmpeqq, pcmpgtq | generic |
| Вставка/извлечение | pinsrb/d/q, pextrb/w/d/q, extractps | generic |
| FMA | vfmadd/fmsub/fnmadd/fnmsub 132/213/231 ps/pd/ss/sd (128-бит) | inline (mul+add, двойное округление); 256-бит — в plan v0.2 |
| Состояние ymm | vzeroall/vzeroupper | inline |

## SSE4.1 / SSE4.2 (без VEX)

| Опкоды | Способ |
|---|---|
| ptest, pcmpestrm/stri, pcmpistrm/stri (экспериментально), crc32 r8/16/32/64 (CRC-32C) | generic |
| pcmpeqq | generic |
| Всё прочее SSE4.1/4.2 — см. строки выше (общие с AVX-версиями) | — |

## Скалярные наборы

| Опкоды | Способ |
|---|---|
| popcnt 16/32 | inline (классическая 2-регистровая схема) |
| popcnt 64 | generic |
| lzcnt | inline (mov -1; bsr; xor W-1) — флаги приближены |
| tzcnt | inline (mov W; bsf) — флаги приближены |
| movbe (load/store, 16/32/64) | inline |
| andn, blsi, blsmsk, blsr | inline (безветвевые, TG-спил при src==dst) |
| rorx, sarx, shlx, shrx | inline (rcx-спил; dst==rcx → generic) |
| bzhi, bextr, pext, pdep, mulx, adcx, adox | generic |
| cpuid, xgetbv | подмена витриной |

## Не эмулируется (trap + диагностика)

* AVX-512: все EVEX-опкоды, k0–k7, zmm (обнаруживаются по размеру/регистру);
* SHA-NI, GFNI, VAES, VPCLMULQDQ-512;
* RDRAND/RDSEED (энтропия), MONITOR/MWAIT вне профиля, MOVDIR*, ENQCMD, AMX;
* xsave/xrstor/xsetbv (кроме подмены xgetbv);
* SSE4a/XOP/FMA4/TBM (AMD-эксклюзивы).

## Статистика выполнения

По выходе процесса печатается:

```
[P.A.S.S.] ── итоги ──────────────────────────────────
  target            : core2
  базовых блоков    : N
  inline-переписано : S (статически)
  generic-эмуляций  : D (динамически)
  trap-срабатываний : T
  без эмуляции      : U (статически; поймает trap)
  top generic: op#1060=10 ...
```
