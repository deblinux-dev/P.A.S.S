/* ============================================================
 * P.A.S.S. — B-2022 (v0.3.2-diag): pass_b2022.c
 * ------------------------------------------------------------
 * Прямой патч массива опций mimalloc v3.5.3 В ПАМЯТИ загруженного
 * образа bun 1.4.5. Причина появления: переменные окружения
 * MIMALLOC_* дошли/не дошли — неизвестно, а вывод mimalloc bun
 * глушит (свой writer), поэтому env-вариант B-2021 недоказуем и
 * не сработал. Патч значений в `mi_options[]` до первой аллокации
 * приложения гарантированно применяется: клиент инициализируется
 * на инъекции, до CRT-старта и первого malloc'а bun.
 *
 * Структура дескриптора (Windows, long=32 бит), stride 32 байта:
 *   +0x00 long value;        +0x04 long init;   (0=UNINIT,2=INITIALIZED)
 *   +0x08 long option_id;    +0x10 const char* name;  +0x18 legacy
 * Таблица: RVA 0x69B6F30, 56 опций.
 *
 * Верификация цели: SizeOfImage==0x6FEF000 && TimeDateStamp==0x6AFEA993
 * (то же, что проверяет probe-alloc B-2019), плюс name-указатель
 * каждого дескриптора обязан указывать на имя опции.
 *
 * Использование (клиентская опция, лаунчер передаёт её клиенту как есть):
 *   pass.exe -target penryn -strace \
 *     -mifix="arena_eager_commit=1,pagemap_commit=1,verbose=2,show_errors=1" \
 *     -- bun_new.exe -e "console.log(12+31)"
 *
 * ПРАВИЛА АДРЕСНОЙ АРИФМЕТИКИ (репрод мимо этой правила = крах клиента):
 *   база модуля — 64-битный HMODULE; `unsigned long` на Win64 —
 *   32 бита, каст базы в unsigned long усёк 0x7ff743e90000 до
 *   0x43e90000 (разыменование = AV внутри клиента). ВСЕ адреса —
 *   только uintptr_t; RVA (младше 4ГиБ) — unsigned long допустим.
 * ============================================================ */
#include "dr_api.h"

#ifdef WINDOWS
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define BUN_SIZEOF_IMAGE 0x6FEF000u
#define BUN_TIMESTAMP    0x6AFEA993u

typedef struct {
    const char*    name;      /* имя опции (без префикса mimalloc_)        */
    unsigned long  desc_rva;  /* RVA дескриптора в образе bun              */
    unsigned long  name_rva;  /* RVA строки имени (для верификации)        */
} mi_opt_entry;

/* Полная таблица `_mi_options[]` bun 1.4.5 (mimalloc v3.5.3 + бун-форк:
 * scavenger, purge_holes*, os_tag 240 и т.д.) */
static const mi_opt_entry mi_opts[] = {
  { "show_errors",                   0x69B6F30, 0x684F420 },
  { "show_stats",                    0x69B6F50, 0x684E4B8 },
  { "verbose",                       0x69B6F70, 0x6883660 },
  { "deprecated_eager_commit",       0x69B6F90, 0x6846D78 },
  { "arena_eager_commit",            0x69B6FB0, 0x6846D90 },
  { "purge_decommits",               0x69B6FD0, 0x684DE68 },
  { "allow_large_os_pages",          0x69B6FF0, 0x68572F0 },
  { "reserve_huge_os_pages",         0x69B7010, 0x68572D8 },
  { "reserve_huge_os_pages_at",      0x69B7030, 0x684C0A0 },
  { "reserve_os_memory",             0x69B7050, 0x6837F60 },
  { "deprecated_segment_cache",      0x69B7070, 0x688E040 },
  { "deprecated_page_reset",         0x69B7090, 0x6847BE0 },
  { "deprecated_abandoned_page_purge",0x69B70B0,0x688E370 },
  { "deprecated_segment_reset",      0x69B70D0, 0x6847BC0 },
  { "deprecated_eager_commit_delay", 0x69B70F0, 0x683B768 },
  { "purge_delay",                   0x69B7110, 0x683B7B8 },
  { "use_numa_nodes",                0x69B7130, 0x68575D0 },
  { "disallow_os_alloc",             0x69B7150, 0x689D508 },
  { "os_tag",                        0x69B7170, 0x6830498 },
  { "max_errors",                    0x69B7190, 0x684F410 },
  { "max_warnings",                  0x69B71B0, 0x6853D00 },
  { "deprecated_max_segment_reclaim",0x69B71D0, 0x686FE98 },
  { "destroy_on_exit",               0x69B71F0, 0x6846868 },
  { "arena_reserve",                 0x69B7210, 0x687F7C8 },
  { "arena_purge_mult",              0x69B7230, 0x68466E0 },
  { "deprecated_purge_extend_delay", 0x69B7250, 0x683B7E0 },
  { "disallow_arena_alloc",          0x69B7270, 0x689D548 },
  { "retry_on_oom",                  0x69B7290, 0x686FC30 },
  { "deprecated_visit_abandoned",    0x69B72B0, 0x6898560 },
  { "guarded_min",                   0x69B72D0, 0x686DB60 },
  { "guarded_max",                   0x69B72F0, 0x683C4B8 },
  { "guarded_precise",               0x69B7310, 0x6883CF0 },
  { "guarded_sample_rate",           0x69B7330, 0x68822A0 },
  { "guarded_sample_seed",           0x69B7350, 0x689AE40 },
  { "generic_collect",               0x69B7370, 0x6849500 },
  { "page_reclaim_on_free",          0x69B7390, 0x688FE78 },
  { "page_full_retain",              0x69B73B0, 0x686DF88 },
  { "page_max_candidates",           0x69B73D0, 0x6854DD8 },
  { "max_vabits",                    0x69B73F0, 0x684E018 },
  { "pagemap_commit",                0x69B7410, 0x6846DA8 },
  { "page_commit_on_demand",         0x69B7430, 0x6893D28 },
  { "page_max_reclaim",              0x69B7450, 0x686FE60 },
  { "page_cross_thread_max_reclaim", 0x69B7470, 0x686FE78 },
  { "allow_thp",                     0x69B7490, 0x6864EF0 },
  { "minimal_purge_size",            0x69B74B0, 0x687E570 },
  { "arena_max_object_size",         0x69B74D0, 0x687E458 },
  { "arena_is_numa_local",           0x69B74F0, 0x6873DC8 },
  { "collect_merges_stats",          0x69B7510, 0x684E4C8 },
  { "snapshot_on_exit",              0x69B7530, 0x6846878 },
  { "scavenger",                     0x69B7550, 0x6860FB0 },
  { "purge_holes",                   0x69B7570, 0x68561E0 },
  { "purge_holes_eager_zero",        0x69B7590, 0x6865DC0 },
  { "purge_holes_min_interval",      0x69B75B0, 0x6872EF0 },
  { "purge_holes_full_every",        0x69B75D0, 0x68385E0 },
  { "purge_holes_large_floor",       0x69B75F0, 0x685D0E8 },
  { "purge_holes_large_floor_epochs",0x69B7610, 0x68537E0 },
};

/* ------------------------------------------------------------
 * вывод в консоль (самодостаточно, без зависимости от pass.c).
 * WriteFile НАПРЯМУЮ — как в veh-дампе segv_dump.c: вызывается
 * из dr_client_main (до app-кода), dr_fprintf не нужен.
 */
static void b2_out(const char* s)
{
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s,
              (DWORD)lstrlenA(s), &written, NULL);
}

static void b2_log_old_new(const char* name, long oldv, long newv)
{
    char buf[160];
    int n = _snprintf(buf, sizeof(buf) - 1,
                      "[P.A.S.S.][MIFIX] mimalloc option '%s': %ld -> %ld\r\n",
                      name, oldv, newv);
    buf[n >= 0 ? n : 0] = 0;
    b2_out(buf);
}

/* ------------------------------------------------------------
 * верификация образа bun 1.4.5 (все чтения — через uintptr_t)
 */
static int b2_verify_bun(uintptr_t base)
{
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt;
    if (dos == NULL || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS*)(base + (uintptr_t)dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    if (nt->OptionalHeader.SizeOfImage != BUN_SIZEOF_IMAGE) return 0;
    if (nt->FileHeader.TimeDateStamp != BUN_TIMESTAMP) return 0;
    return 1;
}

/* ------------------------------------------------------------
 * установка одной опции
 */
static int b2_set_option(uintptr_t base, const mi_opt_entry* e, long value)
{
    uintptr_t desc = base + (uintptr_t)e->desc_rva;
    /* верификация: name-указатель должен указывать на строку имени */
    const char* np = *(const char* volatile*)(desc + 0x10);
    if (np != (const char*)(base + (uintptr_t)e->name_rva)) {
        char buf[200];
        int n = _snprintf(buf, sizeof(buf) - 1,
                          "[P.A.S.S.][MIFIX] ВНИМАНИЕ: layout '%s' не совпал "
                          "(desc RVA=0x%08lX) - пропуск\r\n", e->name, e->desc_rva);
        buf[n >= 0 ? n : 0] = 0;
        b2_out(buf);
        return 0;
    }
    {
        volatile long* pv = (volatile long*)desc;        /* value */
        volatile long* pi = (volatile long*)(desc + 4);  /* init  */
        long old = *pv;
        *pv = value;
        *pi = 2; /* MI_OPTION_INITIALIZED — env для этой опции больше
                    не читается вообще */
        b2_log_old_new(e->name, old, value);
    }
    return 1;
}

/* ------------------------------------------------------------
 * разбор спецификации "имя=значение,имя=значение,..."
 * (разделитель пар — ',' или ';', внутри пары '=' или ':')
 */
void pass_b2022_mifix(const char* spec)
{
    char buf[1024];
    uintptr_t base;
    int applied = 0, failed = 0;
    char* tok;

    if (spec == NULL || spec[0] == 0) return;
    base = (uintptr_t)GetModuleHandleA(NULL);
    if (base == 0) { b2_out("[P.A.S.S.][MIFIX] главный модуль не найден\r\n"); return; }
    if (!b2_verify_bun(base)) {
        b2_out("[P.A.S.S.][MIFIX] образ не bun 1.4.5 (SizeOfImage/"
               "TimeDateStamp не совпали) — опции не применены\r\n");
        return;
    }

    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    for (tok = strtok(buf, ",;"); tok != NULL; tok = strtok(NULL, ",;")) {
        char* eq = strchr(tok, '=');
        char* col = strchr(tok, ':');
        char* sep = NULL;
        long value;
        size_t i, n = sizeof(mi_opts) / sizeof(mi_opts[0]);
        int hit = 0;

        if (eq != NULL && (col == NULL || eq < col)) sep = eq;
        else if (col != NULL) sep = col;
        if (sep == NULL || sep == tok) {
            b2_out("[P.A.S.S.][MIFIX] пропуск сегмента без 'имя=значение': ");
            b2_out(tok); b2_out("\r\n");
            continue;
        }
        *sep = 0;
        value = strtol(sep + 1, NULL, 0);

        for (i = 0; i < n; i++) {
            if (strcmp(mi_opts[i].name, tok) == 0) {
                if (b2_set_option(base, &mi_opts[i], value)) applied++;
                else failed++;
                hit = 1;
                break;
            }
        }
        if (!hit) {
            char ebuf[200];
            int n = _snprintf(ebuf, sizeof(ebuf) - 1,
                              "[P.A.S.S.][MIFIX] опция '%s' не найдена в таблице "
                              "mimalloc\r\n", tok);
            ebuf[n >= 0 ? n : 0] = 0;
            b2_out(ebuf);
            failed++;
        }
    }
    {
        char sbuf[128];
        int n = _snprintf(sbuf, sizeof(sbuf) - 1,
                          "[P.A.S.S.][MIFIX] применено %d, не найдено/ошибок %d\r\n",
                          applied, failed);
        sbuf[n >= 0 ? n : 0] = 0;
        b2_out(sbuf);
    }
}

/* ------------------------------------------------------------
 * Диагностические зонд-точки B-2022 (ставятся штатным механизмом
 * зондов pass_client — как B-2019/B-2020: регистрация RVA, на входе
 * в зонд дампятся rcx/rdx/r8/r9 + ret-адрес).
 *
 * Смысл точек (mimalloc v3.5.3, mi_arenas_page_alloc_fresh, RVA):
 *   0x07E6A  ветка «_mi_page_map_register вернул false» (r11!=0)
 *   0x07E93  ветка «... вернул false» (r11==0, через 0x803E)
 *   0x08010  ветка «commit области страницы вернул false» (call 0x3670)
 *   0x0803E  ветка учёта после несостоявшейся регистрации страницы
 *   0x0B7A0  вход _mi_page_map_register(page)
 *   0x0B970  вход mi_page_map_alloc_submap_at(pmap, idx)
 *   0x02EC0  вход mi_os_prim_alloc(subproc, size, alignment, commit, ...)
 *   0x03670  вход _mi_os_commit(subproc, addr, size, is_zero*)
 *
 * Интерпретация:
 *   - если сработал 0x08010/0x07E6A/0x07E93/0x0803E — мы знаем, КАКОЙ
 *     барьер отверг страницу;
 *   - если 0x02EC0 ни разу не вызван между «scan» и «segget -> NULL» —
 *     отказ происходит ДО обращения к ОС (чисто внутренний);
 *   - если 0x02EC0 вызван и вернул NULL — смотрим memid/GetLastError.
 */
typedef struct {
    unsigned long rva;
    const char*   tag;
} b2_probe_entry;

static const b2_probe_entry b2_probes[] = {
  { 0x07E6A, "B1-fail-r11"  },
  { 0x07E93, "B1-fail-r11z" },
  { 0x08010, "commit-fail"  },
  { 0x0803E, "acct-fail"    },
  { 0x0B7A0, "pagereg-enter"},
  { 0x0B970, "submap-enter" },
  { 0x02EC0, "osalloc-enter"},
  { 0x03670, "oscommit-enter"},
};

int pass_b2022_probe_count(void) {
    return (int)(sizeof(b2_probes) / sizeof(b2_probes[0]));
}
const char* pass_b2022_probe_tag(int i) {
    return b2_probes[i].tag;
}
unsigned long pass_b2022_probe_rva(int i) {
    return b2_probes[i].rva;
}

#else /* !WINDOWS: цели нет — совместимость Linux-сборки CI */

void pass_b2022_mifix(const char* spec) { (void)spec; }
int  pass_b2022_probe_count(void) { return 0; }
const char* pass_b2022_probe_tag(int i) { (void)i; return "?"; }
unsigned long pass_b2022_probe_rva(int i) { (void)i; return 0; }

#endif /* WINDOWS */
