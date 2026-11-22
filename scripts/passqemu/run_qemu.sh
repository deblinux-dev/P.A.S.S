#!/usr/bin/env bash
# run_qemu.sh — батарея P.A.S.S. под qemu-user TCG с честным «старым CPU».
#
# Гость: qemu-x86_64 -cpu Penryn (класс Core 2 Duo E8400: SSE4.1 есть,
# SSE4.2/AVX/AVX2/BMI — нет; CPUID и #UD-гейтинг честные).
# Клиент: drrun + libpass_client.so -target core2.
# Эталоны: снимаются НАТИВНО на современном хосте (Xeon AVX-512), дифф побайтно.
#
#   PHASE=1 bash scripts/passqemu/run_qemu.sh   # контроли + asm + дифф-лёгкие
#   PHASE=2 bash scripts/passqemu/run_qemu.sh   # selftest + тяжёлые C/C++
set -u
PASS_ROOT=/home/z/my-project/clone-pass
QTEST=/home/z/my-project/scripts/qtest
QEMU=/home/z/my-project/scripts/qemu-dl/qemu-root/usr/bin/qemu-x86_64
export LD_LIBRARY_PATH=/home/z/my-project/scripts/qemu-dl/qemu-root/usr/lib/x86_64-linux-gnu
DRRUN=/home/z/my-project/scripts/dr/DynamoRIO-Linux-11.91.20763/bin64/drrun
CLIENT="$PASS_ROOT/build/libpass_client.so"
CPU="${CPU_MODEL:-Penryn}"
PHASE="${PHASE:-1}"
REF="$QTEST/ref"; OUT="$QTEST/out"
mkdir -p "$REF" "$OUT"
PASS_N=0; FAIL_N=0
ok(){ echo "  [OK]   $1"; PASS_N=$((PASS_N+1)); }
bad(){ echo "  [FAIL] $1"; FAIL_N=$((FAIL_N+1)); }
qpass(){ timeout "${QP_TMO:-120}" "$QEMU" -cpu "$CPU" "$DRRUN" -c "$CLIENT" -target core2 -- "$@"; }

cd "$QTEST"

echo "── гостевой CPU: qemu -cpu $CPU, клиент: $(basename "$CLIENT") -target core2"

# собрать все asm-тесты, которых ещё нет (t_lztz/t_bcast/t_s41/t_istr/t_vcmp и др.)
for s in "$PASS_ROOT"/tests/t_*.S; do
  t=$(basename "$s" .S)
  [ -x "$QTEST/$t" ] || { as "$s" -o "$QTEST/$t.o" && ld "$QTEST/$t.o" -o "$QTEST/$t" || true; }
done

if [ "$PHASE" = "1" ]; then
  echo "── контроли: без клиента под $CPU обязателен SIGILL (rc=132):"
  for t in t_pmov2 t_sh256; do
    timeout 60 "$QEMU" -cpu "$CPU" "./$t" >/dev/null 2>&1; rc=$?
    if [ "$rc" = "132" ]; then ok "контроль $t: без клиента SIGILL (rc=132) — #UD-гейтинг жив"
    else bad "контроль $t: rc=$rc, ожидался 132"; fi
  done
  timeout 60 "$QEMU" -cpu "$CPU" "./t_istr" >/dev/null 2>&1; rc=$?
  if [ "$rc" != "0" ]; then ok "контроль t_istr: без клиента rc=$rc (тест сам гейтится по CPUID — тоже корректно)"
  else bad "контроль t_istr: rc=0 без клиента?!"; fi

  echo "── asm-тесты под P.A.S.S. @ $CPU (rc=0):"
  for t in t_pmov2 t_vz t_flags t_msk t_store t_str t_vq t_vq4 t_fmasd t_fam t_sh256 t_vextm t_vdir15 t_sigill; do
    [ -x "$t" ] || continue
    qpass "./$t" >/dev/null 2>&1 && ok "$t" || bad "$t (rc!=0)"
  done

  echo "── сборка C-тестов (нативно, современный gcc):"
  CFLAGS="-O2 -msse4.2 -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -maes -mpclmul -mfma"
  gcc -static $CFLAGS "$PASS_ROOT/tests/data/selftest.c"  -o selftest_s -lm || true
  gcc -static -O2 "$PASS_ROOT/tests/data/tp.c"  -o tp  || true
  gcc -static -O2 "$PASS_ROOT/tests/data/ts2.c" -o ts2 || true
  gcc -static -O2 "$PASS_ROOT/tests/data/ts3.c" -o ts3 || true
  gcc -static -O2 "$PASS_ROOT/tests/data/h3.c"  -o h3  || true
  gcc -static -O2 "$PASS_ROOT/tests/data/h4.c"  -o h4  || true
  gcc -static -O2 "$PASS_ROOT/tests/data/hello.c" -o hello_s || true
  gcc -O1 -msse4.1 "$PASS_ROOT/tests/data/t_r10.c" -o t_r10 || true
  gcc -O1          "$PASS_ROOT/tests/data/t_deadloop.c" -o t_deadloop || true

  echo "── статические приложения (stdout-дифф):"
  for t in h3 h4; do
    n=$(./"$t" 2>/dev/null); p=$(qpass "./$t" 2>/dev/null)
    if [ "$n" = "ok" ] && [ "$p" = "ok" ]; then ok "$t"; else bad "$t (n='$n' p='$p')"; fi
  done
  n=$(./hello_s 2>/dev/null); p=$(qpass ./hello_s 2>/dev/null)
  if [ "$n" = "hello static 42" ] && [ "$p" = "hello static 42" ]; then ok "hello_static"; else bad "hello_static (n='$n' p='$p')"; fi

  echo "── дифф printf/строковые (побайтно):"
  for t in tp ts2 ts3; do
    [ -x "$t" ] || continue
    "./$t" > "$REF/$t.bin" 2>/dev/null
    qpass "./$t" > "$OUT/$t.bin" 2>/dev/null
    cmp -s "$REF/$t.bin" "$OUT/$t.bin" && ok "$t" || bad "$t (вывод различается)"
  done

  echo "── дифф ISA-батареи (побайтно):"
  for t in t_lztz t_r10 t_bcast t_s41 t_istr t_vcmp t_live15 t_sigill; do
    [ -x "$t" ] || continue
    "./$t" > "$REF/$t.bin" 2>/dev/null
    qpass "./$t" > "$OUT/$t.bin" 2>/dev/null
    cmp -s "$REF/$t.bin" "$OUT/$t.bin" && ok "$t" || bad "$t (вывод различается)"
  done

  echo "── broadcast generic-путь (-nobcast) @ $CPU:"
  if [ -x t_bcast ]; then
    ./t_bcast > "$REF/t_bcast_nb.bin" 2>/dev/null
    timeout "${QP_TMO:-120}" "$QEMU" -cpu "$CPU" "$DRRUN" -c "$CLIENT" -nobcast -target core2 -- ./t_bcast > "$OUT/t_bcast_nb.bin" 2>/dev/null
    cmp -s "$REF/t_bcast_nb.bin" "$OUT/t_bcast_nb.bin" && ok "t_bcast -nobcast" || bad "t_bcast -nobcast"
  fi

  echo "── предохранитель trap-цикла t_deadloop (НАТИВНО, fuse→exit 86):"
  if [ -x t_deadloop ]; then
    timeout 30 "$DRRUN" -c "$CLIENT" -target core2 -- "$QTEST/t_deadloop" >/dev/null 2>&1; rc=$?
    [ "$rc" = "86" ] && ok "t_deadloop native (exit 86)" || bad "t_deadloop native: rc=$rc (ожидался 86)"
  fi
fi

if [ "$PHASE" = "2" ]; then
  echo "── тяжёлые C/C++-диффы под $CPU:"
  g++ -O2 -mavx2 -std=c++17 "$PASS_ROOT/tests/data/t_simdutf.cxx"  -o t_simdutf  2>/dev/null || true
  g++ -O2 -mavx2 -std=c++17 "$PASS_ROOT/tests/data/t_blendvar.cxx" -o t_blendvar 2>/dev/null || true
  gcc -O1 -mavx2 -maes "$PASS_ROOT/tests/data/t_vx41.c" -o t_vx41 2>/dev/null || true

  if [ -x t_vx41 ]; then
    ./t_vx41 6 > "$REF/t_vx41.bin" 2>/dev/null
    QP_TMO=240 qpass ./t_vx41 6 > "$OUT/t_vx41.bin" 2>/dev/null
    cmp -s "$REF/t_vx41.bin" "$OUT/t_vx41.bin" && ok "t_vx41 (stage 6: все блоки)" || bad "t_vx41 (вывод различается)"
  fi
  if [ -x t_simdutf ]; then
    ./t_simdutf > "$REF/t_simdutf.bin" 2>/dev/null
    QP_TMO=240 qpass ./t_simdutf > "$OUT/t_simdutf.bin" 2>/dev/null
    cmp -s "$REF/t_simdutf.bin" "$OUT/t_simdutf.bin" && ok "t_simdutf (haswell utf16→utf8, B-2008)" || bad "t_simdutf (вывод различается)"
  fi
  if [ -x t_blendvar ]; then
    ./t_blendvar > "$REF/t_blendvar.bin" 2>/dev/null
    QP_TMO=240 qpass ./t_blendvar > "$OUT/t_blendvar.bin" 2>/dev/null
    cmp -s "$REF/t_blendvar.bin" "$OUT/t_blendvar.bin" && ok "t_blendvar (алиасинг vpblendvb, B-2008)" || bad "t_blendvar (вывод различается)"
  fi

  echo "── KAT selftest под $CPU (rc=0):"
  if [ -x selftest_s ]; then
    QP_TMO=480 qpass ./selftest_s >/dev/null 2>&1 && ok "selftest" || bad "selftest (rc!=0/таймаут)"
  fi
fi

echo
echo "итого PHASE=$PHASE: PASS=$PASS_N FAIL=$FAIL_N"
[ "$FAIL_N" = "0" ]
