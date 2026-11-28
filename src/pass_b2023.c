/* ============================================================
 * P.A.S.S. — B-2023 (v0.3.3-diag): pass_b2023.c
 * ------------------------------------------------------------
 * PAGEMAP-FIX: замена статической page-map карты mimalloc bun
 * 1.4.5 (mimalloc 3.5.3) на динамическую, плюс зонды цепочки
 * init для доказательства места обрыва.
 *
 * КОРНЕВАЯ ПРИЧИНА OOM (разбор логов Прогонов A/B B-2022 от
 * 2026-11-25 + дизасм bun.exe, SizeOfImage 0x6FEF000):
 *
 *  1. Глобал карты: RVA 0x69B76C0 -> СТАТИЧЕСКАЯ карта в .data
 *     по RVA 0x69B7860: count=1, capacity=0x38 (56 байт — только
 *     заголовок; pre-seeded субмапы в [0/2/3/5] недостижимы, т.к.
 *     count=1 отсекает всё, кроме top_idx 0).
 *  2. ВАЖНО (уточнение по логам CI B-2023): mi_init выполняется на
 *     ФАЗЕ ЛОАДЕРА, ДО инициализации DR-клиента (диагностика в
 *     баннере: procinit-lock/flag/pmstate уже ==1 в момент
 *     dr_client_main; старый указатель карты != статическому).
 *     Поэтому strace не видел osalloc 0x90000 (сисколлы лоадера
 *     раньше установки strace-хуков), а зонды init-цепочки (PROBE3)
 *     после старта клиента не срабатывают никогда — оставлены как
 *     страховка на повторный init.
 *     Что было СЛОМАНО: созданная mi_init карта под P.A.S.S.
 *     недостаточна (на машине юзера регистрация top_idx~3190
 *     отказывала: grow вставал на пред-проверке capacity, коммит
 *     карты не вызывался; submap-enter не звался).
 *  3. _mi_page_map_register (0xB7A0): top_idx = page_start >> 29
 *     (512 МиБ на запись верхнего уровня). top_idx >= count ->
 *     map_grow (0x120DFD0) -> ПРЕД-ПРОВЕРКА (capacity-0x38)/8 =
 *     0 < top_idx -> отказ ДО попытки коммита -> return false.
 *     => ЛЮБАЯ страница с VA >= 512 МиБ не регистрируется.
 *  4. Итог: segget -> NULL -> pagemake -> NULL -> pageget/classpg ->
 *     NULL -> grow -> NULL -> report "unable to allocate memory"
 *     -> Zig panic fastfail ("memory allocation of N bytes failed").
 *     Все NT-сисколлы при этом успешны — отказ чисто внутренний.
 *
 * ФИКС: VirtualAlloc 2 МиБ + 64 КиБ, макет 1:1 по дизасму init
 * (0x2500, committed-ветка; lookup 0xC473/0xDFB7 — та же раскладка):
 *   +0x00 count = (cap-0x38)/8+1;   +0x08 capacity = cap;
 *   +0x10..0x28 memid (нули — читает только map_grow, выключен);
 *   +0x28 SRWLOCK (нули = валидный инициализированный);
 *   +0x30 submaps[0] = map+cap (встроенная субмапа #0 в хвосте
 *        аллокации — так делает init: [map+0x30]=map+size);
 *   +0x38.. submaps[i] = NULL (дорастают on-demand: 0xB970 ->
 *        mi_os_prim_alloc 0x2EC0 -> cmpxchg [map+0x30+idx*8]).
 * Записи pagereg [map+0x30+idx*8] при idx<count остаются внутри
 * аллокации: count=(0x210000-0x38)/8+1=270330; max offset
 * 0x30+270329*8=0x20FFC8 < 0x210000. Покрытие VA до ~132 ТиБ
 * (top_idx до 270329 * 512 МиБ) — весь юзерспейс Windows.
 * Замена УКАЗАТЕЛЯ безопасна: 1518 ссылок в коде — все через
 * глобал 0x69B76C0; на сам адрес статической карты ссылок 0.
 * Если mimalloc позже сам создаст карту (init вдруг succeeded) —
 * он перезапишет наш указатель своей (тоже корректно); при отказе
 * init state=1 «отравляется», но наш указатель не трогается.
 *
 * ЗОНДЫ ЦЕПОЧКИ INIT (8 точек; pre-only, те же SizeOfImage/ts):
 *   0x01320 procinit      вход _mi_process_init (не сработал = init не было)
 *   0x01695 pm-gate       проверка FLAG [0x6AC8370]==1
 *   0x016A6 pm-call       достигнут call 0x2500
 *   0x002500 pminit       вход в init page-map
 *   0x002577 pminit-claim cmpxchg state 0->TEB (победа в гонке)
 *   0x002607 pminit-lose  cmpxchg ПРОИГРАН -> тихий выход БЕЗ карты
 *   0x00268D pminit-alloc достигнута аллокация карты (далее osalloc)
 *   0x0028D3 pminit-done  завершение (state->1 либо fail-путь)
 *
 * Опции клиента: -nopagemapfix отключает фикс (бисекция); по
 * умолчанию ВКЛ для проверенного bun 1.4.5 std.
 *
 * ПРАВИЛА АДРЕСНОЙ АРИФМЕТИКИ (как в pass_b2022.c): база модуля —
 * uintptr_t; unsigned long (32 бита на Win64) допустим только для
 * RVA. Нарушение = усечение базы и AV внутри клиента.
 * ============================================================ */
#include "dr_api.h"

#ifdef WINDOWS
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* ── профили поддерживаемых образов bun (pagemap-фикс) ──
 * Статическая карта ищется сигнатурой [1, 0x38, 0, 0, 0x10100000002, 0]
 * в .data; указатель MAP_GLOBAL — единственное qword со значением
 * ImageBase+RVA_MAP_STATIC в образе (обе находки уникальны, сверено
 * на bun_new.exe: MAP_GLOBAL 0x69B76C0 -> MAP_STATIC 0x69B7860).
 * Диагностика procinit/flag/pmstate специфична для профиля 1.4.5. */
typedef struct {
    unsigned long long size_of_image;
    unsigned long long timestamp;
    unsigned long long rva_map_global;
    unsigned long long rva_map_static;
    const char*        label;
} b23_profile;

static const b23_profile b23_profiles[] = {
  /* bun 1.4.5 std (bun_new.exe / bun_baseline.exe) */
  { 0x06FEF000ull, 0x6AFEA993ull, 0x069B76C0ull, 0x069B7860ull,
    "bun 1.4.5 std" },
  /* bun 1.4.4 (встроен в opencode.exe; тот же mimalloc-лейаут,
   * сигнатура статической карты совпадает побайтно) */
  { 0x10B7F000ull, 0x6AD9CC4Full, 0x069A06C0ull, 0x069A0860ull,
    "bun 1.4.4 (opencode)" },
};

#define B23_PROFILES (sizeof(b23_profiles) / sizeof(b23_profiles[0]))

/* геометрия динамической карты */
#define PM_CAP           0x210000ull                       /* 2 МиБ   */
#define PM_ALLOC         (PM_CAP + 0x10000ull)             /* +субмапа0 */
#define PM_COUNT         ((PM_CAP - 0x38) / 8 + 1)         /* 270330  */

/* ── вывод в консоль (самодостаточно, как в pass_b2022.c) ── */
static void b23_out(const char* s)
{
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s,
              (DWORD)lstrlenA(s), &written, NULL);
}

static void b23_log_ll(const char* prefix, unsigned long long v,
                       const char* suffix)
{
    char buf[192];
    int n = _snprintf(buf, sizeof(buf) - 1, "%s%016llx%s",
                      prefix, v, suffix);
    buf[n >= 0 ? n : 0] = 0;
    b23_out(buf);
}

/* ── верификация образа bun по профилям; idx_out = индекс профиля ── */
static int b23_verify_bun(uintptr_t base, int* idx_out)
{
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    const IMAGE_NT_HEADERS* nt;
    unsigned i;
    if (dos == NULL || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (const IMAGE_NT_HEADERS*)(base + (uintptr_t)dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    for (i = 0; i < B23_PROFILES; i++) {
        if (nt->OptionalHeader.SizeOfImage ==
                b23_profiles[i].size_of_image &&
            nt->FileHeader.TimeDateStamp ==
                b23_profiles[i].timestamp) {
            *idx_out = (int)i;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------
 * PAGEMAP-FIX: установка динамической карты.
 * Вызывается из dr_client_main ПОСЛЕ parse_options, до старта
 * приложения (клиент инициализируется на инъекции).
 */
void pass_b2023_pagemap_install(void)
{
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    unsigned long long old_map, old_cnt = 0, old_cap = 0, st1, st2, st3;
    unsigned long long stat_cnt, stat_cap, migrate_n, k;
    unsigned char* map;
    unsigned long long* q;
    char quiet = 0;

    int prof = -1;
    const b23_profile* pr;
    if (base == 0) { b23_out("[P.A.S.S.][PAGEMAP] главный модуль не найден\r\n"); return; }
    if (!b23_verify_bun(base, &prof)) {
        b23_out("[P.A.S.S.][PAGEMAP] образ не входит в список поддерживаемых "
                "bun-профилей (SizeOfImage/TimeDateStamp) — фикс не применён\r\n");
        return;
    }
    pr = &b23_profiles[prof];

    old_map  = *(volatile unsigned long long*)(base + pr->rva_map_global);
    st1 = st2 = st3 = 0;
    if (prof == 0) {
        /* диагностика init доступна только для профиля bun 1.4.5 */
        st1 = *(volatile unsigned long long*)(base + 0x6AC8168ull);
        st2 = *(volatile unsigned long long*)(base + 0x6AC8370ull);
        st3 = *(volatile unsigned long long*)(base + 0x6AC8300ull);
    }
    stat_cnt = *(volatile unsigned long long*)(base + pr->rva_map_static + 0x00);
    stat_cap = *(volatile unsigned long long*)(base + pr->rva_map_static + 0x08);
    if (old_map != 0) {
        old_cnt = *(volatile unsigned long long*)(old_map + 0x00);
        old_cap = *(volatile unsigned long long*)(old_map + 0x08);
    }

    /* карта уже достаточна? (покрывает хотя бы 2 ТиБ: 4096 top-записей)
     * — не трогаем: замена ИЛИ миграция могли бы осиротить страницы,
     * зарегистрированные лоадером до нас.                              */
    if (old_map != 0 && old_cnt >= 4096) {
        quiet = 1;
        b23_out("[P.A.S.S.][PAGEMAP] B-2023: карта уже достаточна — замена не нужна\r\n");
        b23_log_ll("  текущая карта: map=", old_map, "");
        b23_log_ll(" count=", old_cnt, "");
        b23_log_ll(" cap=", old_cap, "\r\n");
        b23_log_ll("  диагностика init: procinit-lock=", st1, "");
        b23_log_ll(" flag=", st2, "");
        b23_log_ll(" pmstate=", st3, "\r\n");
        return;
    }

    map = (unsigned char*)VirtualAlloc(NULL, (SIZE_T)PM_ALLOC,
                                       MEM_RESERVE | MEM_COMMIT,
                                       PAGE_READWRITE);
    if (map == NULL) {
        b23_out("[P.A.S.S.][PAGEMAP] VirtualAlloc отказал — фикс не применён\r\n");
        return;
    }
    /* VirtualAlloc вернул уже занулённую память: memid/SRWLOCK/
     * субмапы остаются нулями — валидное начальное состояние. */
    q = (unsigned long long*)map;
    q[0x00 / 8] = PM_COUNT;              /* count                          */
    q[0x08 / 8] = PM_CAP;                /* capacity                       */
    q[0x30 / 8] = (unsigned long long)(uintptr_t)(map + (size_t)PM_CAP);
                                         /* submaps[0] = встроенная субмапа */
    /* МИГРАЦИЯ: переносим указатели субмап из старой карты (страницы,
     * зарегистрированные до нас — например лоадером — остаются
     * достижимыми). Указатели остаются валидными (память тех карт
     * никто не освобождал).                                            */
    migrate_n = 0;
    if (old_map != 0 && old_cnt > 0) {
        migrate_n = (old_cnt < PM_COUNT) ? old_cnt : PM_COUNT;
        for (k = 0; k < migrate_n; k++) {
            unsigned long long e = *(volatile unsigned long long*)
                (old_map + 0x30 + k * 8);
            if (e != 0)
                *(volatile unsigned long long*)(map + 0x30 + k * 8) = e;
        }
    }
    *(volatile unsigned long long*)(base + pr->rva_map_global) =
        (unsigned long long)(uintptr_t)map;

    b23_out("[P.A.S.S.][PAGEMAP] B-2023: динамическая page-map карта установлена (замена недостаточной)\r\n");
    b23_out("  профиль: "); b23_out(pr->label); b23_out("\r\n");
    b23_log_ll("  старая карта: map=", old_map, "");
    b23_log_ll(" count=", old_cnt, "");
    b23_log_ll(" cap=", old_cap, "\r\n");
    b23_log_ll("  статическая в .data: count=", stat_cnt, "");
    b23_log_ll(" cap=", stat_cap, " (0x38 = только заголовок, рост невозможен)\r\n");
    b23_log_ll("  новая: map=", (unsigned long long)(uintptr_t)map, "");
    b23_log_ll(" count=", PM_COUNT, "");
    b23_log_ll(" cap=", PM_CAP, " (VA до ~132 ТиБ, субмапы on-demand)\r\n");
    b23_log_ll("  мигрировано субмап: ", migrate_n, "\r\n");
    b23_log_ll("  диагностика init: procinit-lock=", st1, "");
    b23_log_ll(" flag=", st2, "");
    b23_log_ll(" pmstate=", st3, "  (pmstate=1 при статической карте = init отказал и отравил state)\r\n");
}

/* ------------------------------------------------------------
 * Зонды цепочки init (pre-only; порядок = индексы колбэков probe.c).
 * НЮАНС: mi_init идёт на фазе лоадера ДО client init, поэтому в
 * нормальном прогоне эти зонды молчат (страховка на повторный init).
 */
typedef struct {
    unsigned long rva;
    const char*   tag;
} b23_probe_entry;

static const b23_probe_entry b23_probes[] = {
  { 0x001320, "procinit"     },  /* вход _mi_process_init                  */
  { 0x001695, "pm-gate"      },  /* FLAG==1?                                */
  { 0x0016A6, "pm-call"      },  /* достигнут call 0x2500                   */
  { 0x002500, "pminit"       },  /* вход в init page-map                    */
  { 0x002577, "pminit-claim" },  /* cmpxchg state 0->TEB                    */
  { 0x002607, "pminit-lose"  },  /* cmpxchg ПРОИГРАН — тихий выход БЕЗ карты */
  { 0x00268D, "pminit-alloc" },  /* достигнута аллокация карты              */
  { 0x0028D3, "pminit-done"  },  /* завершение (state->1 / fail-путь)        */
};

int pass_b2023_probe_count(void) {
    return (int)(sizeof(b23_probes) / sizeof(b23_probes[0]));
}
const char* pass_b2023_probe_tag(int i) {
    return b23_probes[i].tag;
}
unsigned long pass_b2023_probe_rva(int i) {
    return b23_probes[i].rva;
}

#else /* !WINDOWS: цели нет — совместимость Linux-сборки CI */

void pass_b2023_pagemap_install(void) { }
int  pass_b2023_probe_count(void) { return 0; }
const char* pass_b2023_probe_tag(int i) { (void)i; return "?"; }
unsigned long pass_b2023_probe_rva(int i) { (void)i; return 0; }

#endif /* WINDOWS */
