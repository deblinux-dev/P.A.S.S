/* ============================================================
 * P.A.S.S. — B-2026 (diag): pass_b2026.c
 * ------------------------------------------------------------
 * РЕМОНТ INIT-FILL массива индексов (ArrayHashMap-Index / SIMD
 * группы) в bun 1.4.5 std.
 *
 * Симптом (SPINW-дамп локальной сборки, 2026-11-26):
 * спин в SIMD-скане групп метаданных (выход по 0xFF «пусто»
 * или по fingerprint). Дамп при mask=0x1f (len=32):
 *   meta[0..16)  = 39 ff ff 39 21 ff ff 11 ff ff 23 7f 22 68 0f 5e
 *   meta[16..32) = 00 ... 00  (должно быть 0xFF — «пусто»)
 *   meta[32..48) = зеркало слотов 0..15, но с нулями на пустых
 * Т.е. init-fill (0xFF) накрыл только meta[0..16); байты
 * [16..len+16) остались нулями (mimalloc даёт занулённую
 * память), insert уже дописал fp. Скан не видит ни одного
 * 0xFF → бесконечный цикл.
 *
 * ФИКС-ЭКСПЕРИМЕНТ: drwrap pre-hook на scan-функциях (в момент
 * первого вызова map заполнена, но lookup ещё «не сломан»
 * поведенчески): нулевые байты -> 0xFF, зеркало meta[len+i] =
 * meta[i]. Обобщено на любые len (степень двойки, >= 32).
 * Гейт: -norepair.  Лог: [P.A.S.S.][B26].
 * ============================================================ */
#include "dr_api.h"
#include "drwrap.h"

#ifdef WINDOWS
#include <windows.h>
#include <string.h>

#define BUN_SIZEOF_IMAGE_26 0x6FEF000ull

#include "pass_b2026_rvas.h"   /* автосписок кандидатов scan-функций */

static volatile unsigned g_repaired = 0;
static volatile unsigned g_repair_bad = 0;

static int b26_verify_bun(void)
{
    module_data_t *md = dr_get_main_module();
    if (md == NULL) return 0;
    unsigned long long imgsz = (unsigned long long)(md->end - md->start);
    dr_free_module_data(md);
    return imgsz == BUN_SIZEOF_IMAGE_26;
}

static unsigned char g_meta_buf[144 + 16];
static unsigned long long g_meta_g = 0, g_mask_g = 0, g_grp = 0;
static volatile unsigned g_hook_calls = 0;
#define meta_g  g_meta_g
#define mask_g  g_mask_g
#define grp     g_grp

static int b26_valid(unsigned long long meta, unsigned long long mask)
{
    if (meta == 0) return 0;
    unsigned long long len = mask + 1;
    if (len < 32 || len > 128 || (len & (len - 1)) != 0) return 0;
    /* проверка читаемости первых байт индекса */
    unsigned char probe[16];
    if (!dr_safe_read((void*)(uintptr_t)meta, 16, probe, NULL)) return 0;
    return 1;
}

/* pre-hook scan-функции: rcx = map-структура.
 * Раскладки: (A) [map+0x48] = {meta,mask} — сканы 1..3;
 *            (B) map = {meta,mask} — скан 4.                  */
static void b26_scan_pre(void *wrapcxt, void **user_data)
{
    (void)user_data;
    unsigned long long map = (unsigned long long)drwrap_get_arg(wrapcxt, 0);
    /* diag: ВСЕ вызовы с кратным логом первых 60 */
    {
        unsigned c = ++g_hook_calls;
        if (c <= 60) {
            dr_fprintf(STDERR, "[P.A.S.S.][B26] call#%u rcx=%llx\n", c, map);
        }
    }
    if (map == 0) return;

    unsigned long long grp_cand[2] = { 0, 0 };
    unsigned long long via = 0;
    if (dr_safe_read((void*)(uintptr_t)(map + 0x48), 8, &grp_cand[0], NULL)) {
        if (dr_safe_read((void*)(uintptr_t)grp_cand[0], 8, &meta_g, NULL) &&
            dr_safe_read((void*)(uintptr_t)(grp_cand[0] + 8), 8, &mask_g, NULL) &&
            b26_valid(meta_g, mask_g)) {
            grp = grp_cand[0]; via = 0x48;
        }
    }
    if (grp == 0) {
        if (dr_safe_read((void*)(uintptr_t)map, 8, &meta_g, NULL) &&
            dr_safe_read((void*)(uintptr_t)(map + 8), 8, &mask_g, NULL) &&
            b26_valid(meta_g, mask_g)) {
            grp = map; via = 0;
        }
    }
    if (grp == 0) return;
    unsigned long long meta = meta_g, mask = mask_g;
    unsigned long long len = mask + 1;
    /* len — степень двойки в диапазоне [32, 128) (минимальный
     * bit_index=5; большие карты пока не наблюдались) */
    if (len < 32 || len > 128 || (len & (len - 1)) != 0) {
        g_repair_bad++;
        return;
    }

    /* прочитать meta[0..len+16) */
    unsigned int nbytes = (unsigned int)(len + 16);
    if (nbytes > sizeof(g_meta_buf)) return;
    if (!dr_safe_read((void*)(uintptr_t)meta, nbytes, g_meta_buf, NULL)) {
        g_repair_bad++;
        return;
    }
    unsigned char *m = g_meta_buf;
    (void)via;
    /* diag: вызовы с mask != 0x1f (len=128 и др.) + счётчик */
    {
        unsigned c = ++g_hook_calls;
        if (mask != 0x1full) {
            static volatile unsigned logged = 0;
            unsigned l = ++*(volatile unsigned*)&logged;
            (void)l;
            if (g_hook_calls <= 200000) {
                dr_fprintf(STDERR,
                           "[P.A.S.S.][B26] call#%u rcx=%llx grp=%llx via=%llu"
                           " meta=%llx mask=%llx\n",
                           c, map, grp, via, meta, mask);
            }
        }
    }

    /* быстрая проверка «уже чинили/здоров»: второй half не нулевой
     * ИЛИ зеркало уже синхронно — пропускаем без записи          */
    int broken = 0;
    unsigned int half = (unsigned int)(len / 2);
    for (unsigned int i = half; i < len; i++) {
        if (m[i] == 0) { broken = 1; break; }
    }
    if (!broken) {
        /* проверить зеркало: любое расхождение при нулевом зеркале */
        for (unsigned int i = 0; i < 16; i++) {
            if (m[len + i] != m[i] && m[len + i] == 0) { broken = 1; break; }
        }
        if (!broken) return;
    }

    /* РЕМОНТ: нулевые байты индекса -> 0xFF («пусто»); ноль не
     * встречается в живом индексе (fp < 0x80 не может быть 0 у
     * использованного слота после fill-поломки — риск ложного
     * освобождения слота с fp==0 принят как компромисс diag). */
    for (unsigned int i = 0; i < len; i++)
        if (m[i] == 0) m[i] = 0xff;
    for (unsigned int i = 0; i < 16; i++)
        m[len + i] = m[i];

    if (!dr_safe_write((void*)(uintptr_t)meta, nbytes, g_meta_buf, NULL))
        return;

    unsigned n = ++g_repaired;
    if (n <= 64)
        dr_fprintf(STDERR, "[P.A.S.S.][B26] map repaired #%u meta=%llx len=%llu\n",
                   n, meta, len);
}

void pass_b2026_repair_install(void)
{
    if (!b26_verify_bun()) return;
    module_data_t *md = dr_get_main_module();
    if (md == NULL) return;
    app_pc base = md->start;
    dr_free_module_data(md);

    int ok = 0, fail = 0;
    for (unsigned k = 0; k < B26_SCAN_N; k++) {
        app_pc fn = base + (ptr_uint_t)b26_scan_rvas[k];
        if (drwrap_wrap(fn, b26_scan_pre, NULL)) ok++;
        else fail++;
    }
    dr_fprintf(STDERR, "[P.A.S.S.][B26] repair-watch: hooked %d/%u scan-функций (fail %d)\n",
               ok, (unsigned)B26_SCAN_N, fail);
}

#else /* !WINDOWS */

void pass_b2026_repair_install(void) { }

#endif /* WINDOWS */
