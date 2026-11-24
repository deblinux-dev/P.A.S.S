# Покрытие инструкций P.A.S.S. v0.2.8 (§7.4–7.5 раунды #111–#115 + B-2009–B-2018: penryn-профиль, VEX-SSE4.1 inline, close_range, B-2018 OOM-детектор на NtWriteFile)

Легенда: **inline** — безветвевая последовательность в кодовом кэше (быстро);
**generic** — clean-call C-эмулятор (~100 нс); **trap** — не эмулируется,
ловится SIGILL-фоллбеком с диагностикой; **native** — passthrough.
Дифф-тесты: побайтное сравнение native vs pass (asm: t_istr, t_iemu, t_bcast,
t_s41, t_lztz, t_r10; C: t_vx41 — VEX-алиасы/cvt/permil/maskmov, 6 стадий).

## AVX/AVX2 (VEX)

| Группа | Опкоды | Способ |
|---|---|---|
| Арифметика FP/INT 128/256 | vadd/vsub/vmul/vdiv/vmin/vmax ps,pd,ss,sd; vpadd/vpsub/vpadds/vpaddus/vpsubs/vpaddusb/w/vpsubusb/w/vpavg; vpand/vpor/vpxor/vpandn; vpmulhw/lw/huw (§7.4: huw добавлен) | inline (механика NDS/ND2) |
| Перестановки/упаковки | vpunpck*, vpacksswb/dw, vpackuswb, vpshufb, vpalignr, vpsign*, vpabs*, vphadd*, vphsub*, vpmul*, vpmadd*, vpsadbw, vpblendw, vshuf*, vcmpp*, vunpck*, vhadd*, vhsub*, vaddsub* | inline |
| Сдвиги | vpsllw/d/q, vpsrlw/d/q, vpsraw/d, vpslldq, vpsrldq (imm и xmm-счётчик) | inline (VF_SHIFT) |
| «Чистые» | vsqrtp*, vrcpps, vrsqrtp*, vcvtdq2ps, vcvtps2dq, vcvttps2dq, vpshufd, vpshuflw/hw, vcomis*, vucomis*, vcvts*s*→si | inline (VF_ND2) |
| Перемещения | vmovaps/upd/apd/upd, vmovd/q, vmovss/sd (2- и 3-оп.), vmovntp*, vlddqu, vld/stmxcsr, vmovshdup/sldup/ddup (§7.4) | inline (VF_DIR/ND2; 256 — split) |
| Полуходы | vmovlps/hps/lpd/hpd (load-merge и store из тени) | generic (§7.4) |
| Сравнения с imm | vcmpps/pd; vcmpss/sd (#113, §7.5 — mask-compare, флагов нет, DEST[127:64]←SRC1) | inline; generic |
| Конверсии | vcvtsi2ss/sd, vcvtss2sd, vcvtsd2ss | inline; vcvtps2pd (ymm←xmm), vcvtpd2ps, vcvtdq2pd, vcvtpd2dq, vcvttpd2dq (§7.4 — 256-битные формы читают младшую половину источника) | generic |
| Broadcast | vbroadcastss/sd/f128/i128, vpbroadcastb/w/d/q (reg и mem) | inline (стабы 1dst_3src — §7.3) и generic (emu, -nobcast); t_bcast 18 кейсов × default/-nobcast/-inline 0 |
| Комбинирование половин | vinsertf/i128, vextractf/i128 | inline |
| Маскирование бит | vpmovmskb, vmovmskps/pd (128 и 256) | inline (lo\|hi<<n через shadow, флаги сохраняются) |
| Состояние | vzeroupper, vzeroall | inline (64 mov-imm; +pxor-цепочка) |
| Переменные сдвиги | vpsllv*, vpsrlv*, vpsrav* | generic |
| Перестановки cross-lane | vpermd, vpermq, vperm2f128, vperm2i128 | generic |
| Смешение | vpblendd, vblendps/pd (imm), vblendvps/pd (VEX: база = V srcs[0], маска = srcs[2]), blendv*, pblendvb/vpblendvb (B-2008: is_ymm смешивает ОБЕ половины; раньше верх = мусор тени → порча кириллицы в bun/simdutf), vtestps/pd | generic |
| ROUND/DOT | vroundps/pd — inline (B-2014); vroundss/sd, vdpps/dppd, mpsadbw (ymm: у lane1 свои биты imm), phminposuw, insertps | generic + inline |
| maskmov/gather | vpmaskmovd/q (load: srcs=[маска, mem], незаскированные = 0; store: srcs=[данные, маска]), vmaskmovps/pd (store: srcs=[маска, данные] — порядок DR различается по семействам!), vgatherdd/dq/qd/qq, vgatherdps/dpd/qps/qpd | generic (маскированные каналы пропускаются) |
| min/max SSE4.1-набор | pminsb/sd/uw/ud, pmaxsb/sd/uw/ud; VEX-алиасы: vpminsb/sd, vpmaxsb/sd — inline (B-2014), остальные vpmin*/vpmax* | generic + inline |
| Расширения | pmovsx*, pmovzx* (12 вариантов + VEX-алиасы; 256-бит — lane1 читает вторую половину младших 128Б), packusdw, pmulld, pmuldq, pcmpeqq, pcmpgtq; VEX: vpackusdw, vpmuldq, vpmulld — inline (B-2014) | generic + inline |
| Вставка/извлечение | pinsrb/d/q, pextrb/w/d/q, insertps (VEX IR = [imm, V, W]), extractps (VEX IR = [imm, xmm]); VEX vpextrX r32/m, xmm, imm — inline (VF_EXTR, mem-dst напрямую, VEX.256 — выбор половины по imm), vpinsrX — inline (VF_INS: movaps merge + legacy pinsrX) (B-2014) | generic + inline |
| Permil | vpermilps/pd (imm и reg; PD-селектор = бит 1 чётного dword) | generic (§7.4) |
| FMA | vfmadd/fmsub/fnmadd/fnmsub 132/213/231 ps/pd/ss/sd (128 inline — двойное округление; 256 — generic) | inline+generic |
| AES/PCLMUL | aesenc/dec/enclast/declast/imc/keygenassist + VEX-алиасы (vaes*), pclmulqdq/vpclmulqdq | generic |
| Строковые SSE4.2 | pcmpestri/strm, pcmpistri/strm + VEX-формы: полный SDM §4.1 (см. SSE4.2 ниже) | generic (§7.4) |
| Состояние ymm | vzeroall/vzeroupper | inline |

## SSE4.1 / SSE4.2 (без VEX)

| Опкоды | Способ |
|---|---|
| ptest | generic; vptest — inline (B-2014) |
| pcmpestri/strm, pcmpistri/strm (+VEX): точная модель SDM §4.1 — SRC2 = индексируемая строка; валиден элемент строго ДО NUL (ISTR, сам NUL невалиден) или до \|EAX\|/\|EDX\| с насыщением (ESTR); BoolRes[j,i] форсируется ПО ПАРЕ (Table 4-7): any/ranges — любая невалидность пары → false, each — (inv,inv) → true (NUL-трюк $0x3a), ordered — невалидный needle → wildcard true; полярность: − = XOR всех, masked+ ≡ positive, masked− = XOR только валидных SRC2; ZF/SF: ESTR \|EDX\|/\|EAX\| < ne, ISTR — наличие NUL; CF=any(IntRes2), OF=IntRes2[0]; XMM0 — выход strm (bit/unit-mask). Дифф t_istr.S: 230 кейсов × (ECX+EFLAGS+XMM0) побайтно, включая strcspn-паттерн glibc (регресс CI #111 закрыт раундом #112) | generic (§7.4, не «экспериментально») |
| pcmpeqq, pcmpgtq, pmulld, pmuldq, packusdw, mpsadbw, phminposuw, dpps/d, blendps/d, blendv*, pblendvb, pminsb/sd/uw/ud, pmaxsb/sd/uw/ud, pmovsx*, pmovzx*, insertps, extractps, crc32 r8/16/32/64 (CRC-32C) | generic |

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

* AVX-512: все EVEX-опкоды (включая VPSLLVW/RLVW/RAVW — AVX512BW),
  k0–k7, zmm (обнаруживаются по размеру/регистру);
* SHA-NI, GFNI, VAES-512, VPCLMULQDQ-512;
* RDRAND/RDSEED (энтропия), MONITOR/MWAIT вне профиля, MOVDIR*, ENQCMD, AMX;
* xsave/xrstor/xsetbv (кроме подмены xgetbv);
* SSE4a/XOP/FMA4/TBM (AMD-эксклюзивы);
* F16C (vcvtps2ph/vcvtph2ps) — VEX, но emu-реализация не готова
  (округление half-float) — следующий раунд;
* все прочие неизвестные — passthrough + trap (громкая диагностика).

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
