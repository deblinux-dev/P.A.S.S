/* P.A.S.S. — fuzz.c
 * §7.3 fault#1: систематический перебор карт рерайтера. Для каждого
 * опкода из vex_map / fma_tab строим канонические формы операндов
 * (reg/mem/imm × размер 4/8/16/32 × оба порядка imm), придаём
 * инструкции вид app-инструкции (encode→decode → raw bits), прогоняем
 * через pass_bb_event и кодируем КАЖДУЮ инструкцию результата.
 * Запуск: pass -fuzz ... -- <что угодно> (или env PASS_FUZZ=1).
 * Все некодируемые конструкции — одной строкой [FUZZ] FAIL; выход с
 * числом находок. Формы, чей исходный VEX сам не кодируется (неверный
 * порядок операндов для данного опкода), молча пропускаются: рерайтер
 * всегда видит только декодированные канонические инструкции.        */
#include "pass.h"

#ifndef WINDOWS
/* §7.3: фаззер — Linux-dev-инструмент: сигнальный sigsetjmp POSIX,
 * на Windows (MSVC/MinGW) сборка не линкуется — оставляем заглушку;
 * страховка кодируемости на Windows — ловушка UNENCODABLE в pass_ins. */
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

static int fz_failures;
static const char *fz_cur_tag;

/* §7.3: instr_encode на невозможных комбинациях операндов сам может
 * сегфолтнуть (это и есть механизм fault#1) — кодируемость проверяем
 * под sigsetjmp (dev-инструмент, Linux; на Windows ловушка — dr_abort). */
static jmp_buf fz_jmp;
static void fz_segv(int sig)
{
    (void)sig;
    siglongjmp(fz_jmp, 1);
}
static byte *fz_safe_encode(void *dc, instr_t *in, byte *buf)
{
    void (*prev)(int) = signal(SIGSEGV, fz_segv);
    byte *n;
    if (sigsetjmp(fz_jmp, 1) == 0)
        n = instr_encode(dc, in, buf);
    else
        n = NULL;
    signal(SIGSEGV, prev);
    return n;
}

/* кодируемость одной инструкции результата (не прерывает) */
static void fuzz_check_instr(void *dc, instr_t *n)
{
    if (instr_is_label(n) || instr_raw_bits_valid(n))
        return;
    int has_rip_rel = 0;
    for (uint q = 0; q < instr_num_dsts(n) && !has_rip_rel; q++)
        has_rip_rel = opnd_is_near_rel_addr(instr_get_dst(n, q));
    for (uint q = 0; q < instr_num_srcs(n) && !has_rip_rel; q++)
        has_rip_rel = opnd_is_near_rel_addr(instr_get_src(n, q));
    if (has_rip_rel)
        return;
    byte encbuf[32];
    if (fz_safe_encode(dc, n, encbuf) != NULL)
        return;
    dr_fprintf(STDERR, "[FUZZ] FAIL %s: стаб opc=%d", fz_cur_tag,
               instr_get_opcode(n));
    for (uint q = 0; q < instr_num_dsts(n); q++) {
        dr_fprintf(STDERR, " dst");
        opnd_disassemble(dc, instr_get_dst(n, q), STDERR);
    }
    for (uint q = 0; q < instr_num_srcs(n); q++) {
        dr_fprintf(STDERR, " src");
        opnd_disassemble(dc, instr_get_src(n, q), STDERR);
    }
    dr_fprintf(STDERR, "\n");
    fz_failures++;
}

/* сделать из синтетической инструкции настоящую app-образную:
 * encode в буфер → decode обратно (raw bits + канонический порядок
 * операндов, ровно как у декодированной инструкции приложения)       */
static instr_t *fuzz_realify(void *dc, instr_t *in)
{
    byte tmp[32];
    byte *n = fz_safe_encode(dc, in, tmp);
    if (n == NULL) {
        /* форма невалидна для данного опкода — пропустить тихо */
        instr_destroy(dc, in);
        return NULL;
    }
    size_t len = (size_t)(n - tmp);
    instr_destroy(dc, in);
    instr_t *dec = instr_create(dc);
    if (decode(dc, tmp, dec) == NULL) {
        instr_destroy(dc, dec);
        return NULL;
    }
    return dec;
}

static opnd_t fz_mem(opnd_size_t sz)
{
    return opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, 0x40, sz);
}
static opnd_t fz_reg(int idx, opnd_size_t sz)
{
    opnd_t o = opnd_create_reg(DR_REG_XMM0 + (idx & 15));
    opnd_set_size(&o, sz);
    return o;
}

/* один кейс: vex_op + операнды → realify → полный диспетчер → проверка */
static void fuzz_case(void *dc, int vex_op, const char *name,
                      int ndst, opnd_t d0, int nsrc,
                      opnd_t s0, opnd_t s1, opnd_t s2)
{
    fz_cur_tag = name;
    /* трассировка кейсов: последний напечатанный — виновник краша */
    dr_fprintf(STDERR, "[FUZZ] case %s:", name);
    opnd_disassemble(dc, d0, STDERR);
    for (int q = 0; q < nsrc; q++) {
        dr_fprintf(STDERR, " ");
        opnd_disassemble(dc, (q == 0) ? s0 : (q == 1) ? s1 : s2, STDERR);
    }
    dr_fprintf(STDERR, "\n");
    dr_flush_file(STDERR);
    instr_t *in = instr_create(dc);
    instr_set_opcode(in, vex_op);
    instr_set_num_opnds(dc, in, ndst, nsrc);
    if (ndst > 0) instr_set_dst(in, 0, d0);
    if (nsrc > 0) instr_set_src(in, 0, s0);
    if (nsrc > 1) instr_set_src(in, 1, s1);
    if (nsrc > 2) instr_set_src(in, 2, s2);
    in = fuzz_realify(dc, in);
    if (in == NULL) { fz_cur_tag = NULL; return; }
    instrlist_t *ilist = instrlist_create(dc);
    instrlist_append(ilist, in);
    pass_bb_event(dc, NULL, ilist, false, false);
    for (instr_t *n = instrlist_first(ilist); n != NULL;
         n = instr_get_next(n))
        fuzz_check_instr(dc, n);
    instrlist_clear_and_destroy(dc, ilist);
    fz_cur_tag = NULL;
}

/* матрица форм для одной записи карты (128-бит) */
static void fuzz_vex_128(void *dc, int vex_op, const char *name)
{
    opnd_t imm = OPND_CREATE_INT8(0xaa);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_reg(3, 16), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_mem(16), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_reg(3, 8), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_mem(8), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, imm, fz_reg(3, 16), fz_reg(2, 16));
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(3, 16), imm, fz_reg(2, 16));
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_reg(3, 4), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 2, fz_reg(2, 16), fz_mem(4), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 1, fz_reg(2, 16), fz_reg(3, 16), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 16), 1, fz_mem(16), fz_reg(3, 16), imm);
    fuzz_case(dc, vex_op, name, 1, fz_mem(16), 1, fz_reg(2, 16), fz_reg(3, 16), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 8), 2, fz_reg(2, 8), fz_reg(3, 8), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 8), 2, fz_reg(2, 8), fz_mem(8), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 4), 2, fz_reg(2, 4), fz_reg(3, 4), imm);
    fuzz_case(dc, vex_op, name, 1, fz_reg(1, 4), 2, fz_reg(2, 4), fz_mem(4), imm);
}

/* матрица форм для 256-бит */
static void fuzz_vex_256(void *dc, int vex_op, const char *name)
{
    opnd_t imm = OPND_CREATE_INT8(0xaa);
    char tag[80];
    dr_snprintf(tag, sizeof(tag), "%s/ymm", name);
    fz_cur_tag = tag;
    fuzz_case(dc, vex_op, tag, 1, fz_reg(1, 32), 2, fz_reg(2, 32), fz_reg(3, 32), imm);
    fuzz_case(dc, vex_op, tag, 1, fz_reg(1, 32), 2, fz_reg(2, 32), fz_mem(32), imm);
    fuzz_case(dc, vex_op, tag, 1, fz_reg(1, 32), 2, imm, fz_reg(3, 32), fz_reg(2, 32));
    fuzz_case(dc, vex_op, tag, 1, fz_reg(1, 32), 1, fz_reg(2, 32), fz_reg(3, 32), imm);
    fuzz_case(dc, vex_op, tag, 1, fz_mem(32), 1, fz_reg(2, 32), fz_reg(3, 32), imm);
    fz_cur_tag = NULL;
}

void pass_fuzz_run(void)
{
    void *dc = dr_get_current_drcontext();
    pass_ensure_tls(dc);
    dr_fprintf(STDERR, "[FUZZ] старт перебора карт рерайтера\n");
    dr_flush_file(STDERR);

    int n = 0;
    const vex_map_ent_t *m = vex_map_all(&n);
    for (int k = 0; k < n; k++) {
        char name[48];
        dr_snprintf(name, sizeof(name), "op%d", m[k].vex_op);
        fuzz_vex_128(dc, m[k].vex_op, name);
        fuzz_vex_256(dc, m[k].vex_op, name);
    }
    typedef struct { int vex_op; int kind; int form; int packed; int is_pd; } fz_fma_ent_t;
    const fz_fma_ent_t *f = (const fz_fma_ent_t *)fma_tab_all(&n);
    for (int k = 0; k < n; k++) {
        char name[48];
        dr_snprintf(name, sizeof(name), "fma%d", f[k].vex_op);
        opnd_t imm = OPND_CREATE_INT8(0);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 16), 2,
                  fz_reg(2, 16), fz_reg(3, 16), imm);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 16), 2,
                  fz_reg(2, 16), fz_mem(16), imm);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 8), 2,
                  fz_reg(2, 8), fz_reg(3, 8), imm);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 8), 2,
                  fz_reg(2, 8), fz_mem(8), imm);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 4), 2,
                  fz_reg(2, 4), fz_reg(3, 4), imm);
        fuzz_case(dc, f[k].vex_op, name, 1, fz_reg(1, 4), 2,
                  fz_reg(2, 4), fz_mem(4), imm);
    }

    dr_fprintf(STDERR, "[FUZZ] итог: %d некодируемых конструкций\n",
               fz_failures);
    dr_flush_file(STDERR);
    dr_exit_process(fz_failures > 125 ? 125 : fz_failures);
}

#else  /* WINDOWS */
void pass_fuzz_run(void)
{
    /* не вызывается (запуск только клиентской опцией -fuzz на Linux) */
}
#endif
