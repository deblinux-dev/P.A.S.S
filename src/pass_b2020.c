/* ═══════════════════════════════════════════════════════════════════════
 * pass_b2020.c — (C) P.A.S.S., сборка B-2020 (v0.3.0-diag)
 *
 * Назначение: решающая диагностика OOM bun ('memory allocation of 5085
 * bytes failed'). Расшифровка op-номеров из DIAG B-2019 (таблица
 * dr_ir_opcodes_x86.h, кросс-чек op#686=vaddsd / op#728=vmovd /
 * op#732=vpcmpeqb) показала: финальное окно перед OOM — это claim/scan
 * битовой карты страниц аллокатора bun (RVAs из разбора .pdata B-2019:
 * claim 0x8810 = CAS + POPCNT + SETE, scan 0x9100 = TZCNT):
 *   popcnt (op#539) @ bun+0x8886/+0x88e6 (claim) + +0x91cc/+0x9357 (scan)
 *   tzcnt  (op#1059) @ bun+0x9237/+0x93bb (scan) + +0xcebc/+0x1c9dxx
 * повторённый 3-4 раза = ровно столько же попыток segget. Отказ —
 * внутренний (все NT-вызовы SUCCESS): scan не находит свободную
 * страницу свежего сегмента. Модуль и есть ловушка на этот путь.
 *
 * Модуль даёт четыре инструмента:
 *   1) BITOP-трейс: каждый emu tzcnt/popcnt/lzcnt записывает в кольцо
 *      (opc, PC, ширина, src, imm, результат, флаги до/после) + СРАВНИВАЕТ
 *      результат и CF/ZF с поразрядным эталоном (SWAR — без аппаратного
 *      popcnt/ctz, гвард scan_baseline не сработает). MISMATCH печатается
 *      немедленно. Дамп кольца — только в момент пойманного OOM.
 *   2) CODE-дамп: 0x24 байта кода приложения по PCs из кольца и по
 *      адресам 9 зондов (при установке зондов вызвать
 *      pass_b2020_probe_addr()). Дизасм делаем офлайн из лога.
 *   3) SEG-FORENSIC: дамп сегмента аллокатора (0x000..0x4C0), heap
 *      (+0x400..+0x580) и заголовка закоммиченной страницы — вызывается
 *      из «segget -> NULL» (первый отказ); база сегмента запоминается
 *      в зонде findpg, адрес коммита 0x10000 — из SC 24 (note_commit).
 *   4) PRB-rawargs: сырые rcx/rdx/r8/r9 + [rsp](ret) + [rsp+28/30/38]
 *      для каждого зондового вызова — переотображение конвенций.
 *
 * Зависимости: только dr_api.h. Компилируется тем же флаг-набором, что
 * и остальной клиент (SWAR-попкаунт — гвард чист).
 * ═══════════════════════════════════════════════════════════════════════ */

#include "dr_api.h"
#include <string.h>
#include <stdint.h>

/* ── op-номера (dr_ir_opcodes_x86.h, декодировано офлайн) ── */
#define B2_OPC_POPCNT   539
#define B2_OPC_LZCNT    544
#define B2_OPC_TZCNT    1059

/* флаг-биты в сыром eflags-образе (BUF_FLAGS) */
#define B2_FL_CF        0x001u
#define B2_FL_ZF        0x040u
#define B2_FL_SF        0x080u

#define B2_RING_MAX     128
#define B2_PC_MAX       8
#define B2_PROBE_MAX    16

typedef struct {
    uint32_t opc;
    uint32_t w;          /* ширина операции, бит */
    uint64_t pc;         /* PC, как передал хук (pc_next или app-PC) */
    uint64_t s1;         /* исходное значение источника */
    uint64_t s2;         /* второй источник / imm */
    uint64_t res;        /* результат эмуляции (после записи в dst) */
    uint32_t fl0, fl1;   /* BUF_FLAGS до/после */
    uint32_t match;      /* 1 = совпал с эталоном, 2 = MISMATCH, 0 = нет эталона */
} b2_rec_t;

static b2_rec_t b2_ring[B2_RING_MAX];
static uint32_t b2_ring_head = 0;      /* следующий индекс записи */
static uint32_t b2_ring_total = 0;     /* всего записано (для счётчика) */
static int      b2_on = 0;             /* трейс включён после init */
static int      b2_dumped = 0;
static int      b2_segdumped = 0;

static uint64_t b2_pcs[B2_PC_MAX];     /* уникальные PCs для code-дампа */
static uint32_t b2_pc_n = 0;

static uint64_t b2_probe_addrs[B2_PROBE_MAX];
static const char* b2_probe_names[B2_PROBE_MAX];
static uint32_t b2_probe_n = 0;

static uint64_t b2_commit_base = 0;    /* последний SC 24-коммит 0x10000 */
static uint64_t b2_commit_size = 0;

/* ── SWAR-эталоны: БЕЗ аппаратного popcnt/tzcnt (гвард scan_baseline) ── */
static uint64_t b2_popcount64(uint64_t x)
{
    x -= (x >> 1) & 0x5555555555555555ull;
    x  = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x  = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
    return (x * 0x0101010101010101ull) >> 56;
}
static int b2_ctz64(uint64_t v)          /* v != 0 */
{
    return (int)b2_popcount64((v & (~v + 1)) - 1);
}
static int b2_clz64(uint64_t v)          /* v != 0 */
{
    v |= v >> 1; v |= v >> 2; v |= v >> 4;
    v |= v >> 8; v |= v >> 16; v |= v >> 32;
    return 64 - (int)b2_popcount64(v);
}

/* эталонный результат emu-операции; нет эталона → 0 */
static int b2_ref(uint32_t opc, uint32_t w, uint64_t s1, uint64_t s2,
                  uint64_t* ref, int* ref_cf, int* ref_zf)
{
    (void)s2;
    uint64_t mask = (w == 64) ? ~(uint64_t)0 : (((uint64_t)1 << w) - 1);
    uint64_t v = s1 & mask;
    *ref_cf = -1; *ref_zf = -1;
    switch (opc) {
    case B2_OPC_POPCNT:
        *ref = b2_popcount64(v);
        *ref_zf = (v == 0);
        return 1;
    case B2_OPC_TZCNT:
        *ref = (v == 0) ? (uint64_t)w : (uint64_t)b2_ctz64(v);
        *ref_cf = (v == 0);
        *ref_zf = (*ref == 0);
        return 1;
    case B2_OPC_LZCNT: {
        if (v == 0) { *ref = (uint64_t)w; }
        else { *ref = (uint64_t)(b2_clz64(v) - (64 - w)); }
        *ref_cf = (v == 0);
        *ref_zf = (*ref == 0);
        return 1;
    }
    default:
        return 0;
    }
}

static const char* b2_opname(uint32_t opc)
{
    switch (opc) {
    case B2_OPC_POPCNT: return "popcnt";
    case B2_OPC_LZCNT:  return "lzcnt";
    case B2_OPC_TZCNT:  return "tzcnt";
    }
    return "?";
}

/* ── API ─────────────────────────────────────────────────────────────── */

void pass_b2020_enable(void) { b2_on = 1; }

/* регистрация адреса зонда для code-дампа */
void pass_b2020_probe_addr(const char* name, uint64_t addr)
{
    if (b2_probe_n < B2_PROBE_MAX && addr != 0) {
        b2_probe_names[b2_probe_n] = name;
        b2_probe_addrs[b2_probe_n] = addr;
        b2_probe_n++;
    }
}

/* запоминание последнего SC 24-коммита (из strace: post, res==0).
 * Вызывается для размера 0x10000 — коммит страницы сегмента. */
void pass_b2020_note_commit(uint64_t base, uint64_t size)
{
    b2_commit_base = base;
    b2_commit_size = size;
}

/* хук из gpr_popcnt64 / gpr_lzcnt / gpr_tzcnt (emu.c).
 * s1/s2 — значения источников ДО вычисления; res — ПОСЛЕ записи в dst;
 * fl0/fl1 — BUF_FLAGS до/после. pc — любой стабильный app-PC (pc_next
 * допустим; дамп кода ведётся от pc-8).                              */
void pass_b2020_bitop(uint32_t opc, uint64_t pc, uint32_t w,
                      uint64_t s1, uint64_t s2, uint64_t res,
                      uint32_t fl0, uint32_t fl1)
{
    b2_rec_t* r;
    uint64_t ref = 0;
    int ref_cf, ref_zf, have_ref, ok = 1;

    if (!b2_on) return;

    r = &b2_ring[b2_ring_head];
    r->opc = opc; r->w = w; r->pc = pc;
    r->s1 = s1; r->s2 = s2; r->res = res;
    r->fl0 = fl0; r->fl1 = fl1;

    have_ref = b2_ref(opc, w, s1, s2, &ref, &ref_cf, &ref_zf);
    if (have_ref) {
        ok = (res == ref);
        if (ok && ref_cf >= 0)
            ok = ((fl1 & B2_FL_CF) != 0) == (ref_cf != 0);
        if (ok && ref_zf >= 0)
            ok = ((fl1 & B2_FL_ZF) != 0) == (ref_zf != 0);
        r->match = ok ? 1 : 2;
        if (!ok) {
            {
                uint64_t b2_m = (w == 64) ? ~(uint64_t)0 : (((uint64_t)1 << w) - 1);
            dr_fprintf(STDERR,
                "[P.A.S.S.][B2][!!] MISMATCH %s(op#%u) w=%u src=0x%012llx res=0x%llx ref=0x%llx "
                "cf:emu=%u ref=%d zf:emu=%u ref=%d @0x%012llx\n",
                b2_opname(opc), (unsigned)opc, (unsigned)w,
                (unsigned long long)(s1 & b2_m),
                (unsigned long long)res, (unsigned long long)ref,
                (unsigned)((fl1 & B2_FL_CF) != 0), ref_cf,
                (unsigned)((fl1 & B2_FL_ZF) != 0), ref_zf,
                (unsigned long long)pc);
            dr_flush_file(STDERR);
            }
        }
    } else {
        r->match = 0;
    }

    b2_ring_head = (b2_ring_head + 1) % B2_RING_MAX;
    if (b2_ring_total < B2_RING_MAX) b2_ring_total++;
}

/* уникальные PCs для code-дампа */
static void b2_note_pc(uint64_t pc)
{
    uint32_t i;
    if (pc == 0) return;
    for (i = 0; i < b2_pc_n; i++)
        if (b2_pcs[i] == pc) return;
    if (b2_pc_n < B2_PC_MAX) b2_pcs[b2_pc_n++] = pc;
}

static void b2_code_dump(uint64_t pc)
{
    unsigned char buf[0x24];
    size_t done = 0;
    int i;
    uint64_t start = (pc >= 8) ? (pc - 8) : pc;
    if (dr_safe_read((void*)(uintptr_t)start, sizeof(buf), buf, &done) &&
        done == sizeof(buf)) {
        dr_fprintf(STDERR, "[P.A.S.S.][B2][CODE] @0x%012llx:", (unsigned long long)start);
        for (i = 0; i < (int)sizeof(buf); i++)
            dr_fprintf(STDERR, " %02x", buf[i]);
        dr_fprintf(STDERR, "\n");
    } else {
        dr_fprintf(STDERR, "[P.A.S.S.][B2][CODE] @0x%012llx: <недоступно, done=%u>\n",
                   (unsigned long long)start, (unsigned)done);
    }
}

/* дамп qword-строки через dr_safe_read (fault-безопасно) */
static void b2_qline(uint64_t addr, uint32_t n)
{
    uint32_t k;
    dr_fprintf(STDERR, "  +0x%04x:", (unsigned)(addr & 0xFFFF));
    for (k = 0; k < n; k++) {
        uint64_t v = 0;
        size_t done = 0;
        if (dr_safe_read((void*)(uintptr_t)(addr + k * 8), 8, &v, &done) && done == 8)
            dr_fprintf(STDERR, " %016llx", (unsigned long long)v);
        else
            dr_fprintf(STDERR, " ????");
    }
    dr_fprintf(STDERR, "\n");
}

/* SEG-FORENSIC: из печати «segget -> NULL» (первый отказ).
 * heap_ptr — указатель кучи из текущего дампа B-2019; seg_base — база
 * сегмента (аргумент findpg текущей попытки; 0 = не известно);
 * commit_addr — адрес последнего SC 24-коммита (0x…0000+size; 0 = нет). */
void pass_b2020_seg_dump(uint64_t heap_ptr, uint64_t seg_base, uint64_t commit_addr)
{
    uint32_t row;
    if (b2_segdumped) return;
    b2_segdumped = 1;

    if (commit_addr == 0 && b2_commit_base != 0) {
        commit_addr = b2_commit_base;
        dr_fprintf(STDERR, "[P.A.S.S.][B2][SEG] коммит из SC 24: size=0x%llx\n",
                   (unsigned long long)b2_commit_size);
    }

    dr_fprintf(STDERR, "[P.A.S.S.][B2][SEG] seg=0x%012llx heap=0x%012llx commit=0x%012llx\n",
               (unsigned long long)seg_base, (unsigned long long)heap_ptr,
               (unsigned long long)commit_addr);
    dr_fprintf(STDERR, "[P.A.S.S.][B2][SEG] сегмент 0x000..0x4C0 (qwords):\n");
    for (row = 0; row < 6; row++)
        b2_qline(seg_base + row * 0x80, 16);           /* +0x000..+0x300 */
    for (row = 0; row < 2; row++)
        b2_qline(seg_base + 0x400 + row * 0x40, 8);    /* +0x400..+0x480 */
    b2_qline(seg_base + 0x4c0, 8);                     /* +0x4C0 */

    dr_fprintf(STDERR, "[P.A.S.S.][B2][SEG] heap +0x400..+0x580:\n");
    for (row = 0; row < 3; row++)
        b2_qline(heap_ptr + 0x400 + row * 0x80, 16);   /* +0x400..+0x500 */
    b2_qline(heap_ptr + 0x560, 4);                     /* +0x560 */

    if (commit_addr != 0) {
        dr_fprintf(STDERR, "[P.A.S.S.][B2][SEG] заголовок коммита:\n");
        b2_qline(commit_addr, 8);
    }
    dr_flush_file(STDERR);
}

/* PRB-rawargs: из печати зондов. xsp — app rsp в точке зонда
 * (dr_get_mcontext().xsp; [xsp]=retaddr, [xsp+0x28]=arg5, …). */
void pass_b2020_prb(const char* name, uint64_t rcx, uint64_t rdx,
                    uint64_t r8, uint64_t r9, uint64_t xsp)
{
    uint64_t ret = 0, a5 = 0, a6 = 0, a7 = 0;
    size_t done = 0;
    if (!b2_on) return;
    if (xsp != 0) {
        dr_safe_read((void*)(uintptr_t)xsp, 8, &ret, &done);
        dr_safe_read((void*)(uintptr_t)(xsp + 0x28), 8, &a5, &done);
        dr_safe_read((void*)(uintptr_t)(xsp + 0x30), 8, &a6, &done);
        dr_safe_read((void*)(uintptr_t)(xsp + 0x38), 8, &a7, &done);
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.][B2][PRB] %-8s rcx=0x%012llx rdx=0x%012llx r8=0x%012llx r9=0x%012llx "
               "a5=0x%012llx a6=0x%012llx a7=0x%012llx ret=0x%012llx\n",
               name,
               (unsigned long long)rcx, (unsigned long long)rdx,
               (unsigned long long)r8,  (unsigned long long)r9,
               (unsigned long long)a5,  (unsigned long long)a6,
               (unsigned long long)a7,  (unsigned long long)ret);
}

/* дамп всего кольца + кода — из точки поимки OOM ([OOM] паника …). */
void pass_b2020_oom_dump(void)
{
    uint32_t i, k;
    if (b2_dumped) return;
    b2_dumped = 1;

    dr_fprintf(STDERR, "[P.A.S.S.][B2] === BITOP-КОЛЬЦО (всего %u, показ до %u) ===\n",
               (unsigned)b2_ring_total, (unsigned)b2_ring_total);
    k = (b2_ring_total >= B2_RING_MAX) ? b2_ring_head : 0;
    for (i = 0; i < b2_ring_total; i++) {
        b2_rec_t* r = &b2_ring[(k + i) % B2_RING_MAX];
        dr_fprintf(STDERR,
                   "[P.A.S.S.][B2] %s(op#%u) w=%u pc=0x%012llx src=0x%012llx imm=0x%llx "
                   "res=0x%llx fl %08x->%08x m=%u\n",
                   b2_opname(r->opc), (unsigned)r->opc, (unsigned)r->w,
                   (unsigned long long)r->pc,
                   (unsigned long long)r->s1, (unsigned long long)r->s2,
                   (unsigned long long)r->res,
                   (unsigned)r->fl0, (unsigned)r->fl1, (unsigned)r->match);
        b2_note_pc(r->pc);
    }
    dr_fprintf(STDERR, "[P.A.S.S.][B2] === CODE по PCs кольца ===\n");
    for (i = 0; i < b2_pc_n; i++)
        b2_code_dump(b2_pcs[i]);
    dr_fprintf(STDERR, "[P.A.S.S.][B2] === CODE по адресам зондов ===\n");
    for (i = 0; i < b2_probe_n; i++) {
        dr_fprintf(STDERR, "[P.A.S.S.][B2] зонд %s:", b2_probe_names[i]);
        b2_code_dump(b2_probe_addrs[i]);
    }
    dr_flush_file(STDERR);
}
