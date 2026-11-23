/* P.A.S.S. — segv_dump.c (временная диагностика, LINUX only)
 * Перехват SIGSEGV: печатает RIP/RSP/fault и все значения стека,
 * попадающие в исполняемые диапазоны загруженных модулей.
 * Позволяет вручную восстановить цепочку вызовов там, где релизный
 * DR печатает только регистры. Устанавливается ПОСЛЕ обработчика DR,
 * поэтому вытесняет его (наш клиент инициализируется позже).       */
#define _GNU_SOURCE
#include "pass.h"
#ifdef LINUX
#include <signal.h>
#include <ucontext.h>
#include <link.h>
#include <unistd.h>
#include <string.h>

typedef struct { ptr_uint_t lo, hi; } range_t;
static range_t g_exec[64];
static int     g_nexec;

static int phdr_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size; (void)data;
    for (int p = 0; p < info->dlpi_phnum; p++) {
        if (info->dlpi_phdr[p].p_type == PT_LOAD &&
            (info->dlpi_phdr[p].p_flags & PF_X)) {
            if (g_nexec < 64) {
                g_exec[g_nexec].lo = (ptr_uint_t)info->dlpi_addr +
                                     info->dlpi_phdr[p].p_vaddr;
                g_exec[g_nexec].hi = g_exec[g_nexec].lo +
                                     info->dlpi_phdr[p].p_memsz;
                g_nexec++;
            }
        }
    }
    return 0;
}

static int in_exec(ptr_uint_t v)
{
    for (int k = 0; k < g_nexec; k++)
        if (v >= g_exec[k].lo && v < g_exec[k].hi)
            return 1;
    return 0;
}

static void pass_segv_handler(int sig, siginfo_t *si, void *uc)
{
    ucontext_t *u = (ucontext_t *)uc;
    ptr_uint_t rip = (ptr_uint_t)u->uc_mcontext.gregs[REG_RIP];
    ptr_uint_t rsp = (ptr_uint_t)u->uc_mcontext.gregs[REG_RSP];
    dr_fprintf(STDERR, "[P.A.S.S.][SEGV] sig=%d rip=" PIFX " fault=" PIFX
                       " rsp=" PIFX "\n", sig, rip, (ptr_uint_t)si->si_addr, rsp);
    /* ближайший модуль для RIP */
    for (int k = 0; k < g_nexec; k++)
        if (rip >= g_exec[k].lo && rip < g_exec[k].hi)
            dr_fprintf(STDERR, "[P.A.S.S.][SEGV] rip in mod+%d (base " PIFX ")\n",
                       (int)(rip - g_exec[k].lo), g_exec[k].lo);
    /* скан стека: кодовые указатели = кандидаты в адреса возврата */
    int printed = 0;
    for (ptr_uint_t sp = rsp & ~(ptr_uint_t)7; sp < rsp + 0x1000 && printed < 40;
         sp += 8) {
        ptr_uint_t v = *(ptr_uint_t *)sp;
        if (in_exec(v)) {
            dr_fprintf(STDERR, "[P.A.S.S.][SEGV] stk " PIFX ": " PIFX
                               " (mod+" PIFX ")\n",
                       sp, v, v - g_exec[0].lo);
            printed++;
        }
    }
    dr_flush_file(STDERR);
    _exit(78);
}

void pass_segv_dump_install(void)
{
    dl_iterate_phdr(phdr_cb, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = pass_segv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
#endif /* LINUX */

/* ════════ Windows: VEH-лог исключений (§7.3 охота fault#1) ════════
 * Два слоя:
 *  (1) КОЛЬЦО: каждое AV (0xC0000005) и #UD (0xC000001D) пишет
 *      {code, rip, access} в статическое кольцо 32 слотов — голая
 *      запись памяти, без DR API, без printf, безопасно в VEH.
 *  (2) ДАМП: если AV с rip ВНУТРИ dynamorio.dll бьёт по адресу
 *      < 0x10000 (near-NULL — сигнатура фатального encode-to-NULL,
 *      fault#1), печатаем всё кольцо через WriteFile НАПРЯМУЮ:
 *      dr_fprintf идёт через vsnprintf-машинарию DR — сам участок
 *      краша. Никаких DR-вызовов в printing-пути.
 *  Плюс прежнее: лог фолтов с rip в клиенте (до 24 строк, budget). */
#ifdef WINDOWS
#include <windows.h>

typedef struct {
    unsigned long long rip;
    unsigned long long acc;
    unsigned int       code;
} veh_ent_t;

static veh_ent_t veh_ring[32];
static volatile long veh_ring_next;
static volatile long veh_ring_cnt;
static volatile long veh_dumped;   /* фатальный дамп — один раз */
static volatile long veh_budget;   /* живых строк напечатано */
static void *veh_client_lo, *veh_client_hi;
static void *veh_dr_lo, *veh_dr_hi;
static HANDLE veh_errh;

static void veh_write(const char *s, unsigned len)
{
    DWORD wr = 0;
    if (veh_errh != NULL && veh_errh != INVALID_HANDLE_VALUE)
        WriteFile(veh_errh, s, len, &wr, NULL);
}

static void veh_hex(char **p, unsigned long long v)
{
    static const char d[] = "0123456789abcdef";
    char *o = *p;
    int k;
    for (k = 60; k >= 0; k -= 4)
        *o++ = d[(v >> k) & 0xF];
    *p = o;
}

static void veh_line(const char *tag, unsigned int code,
                     unsigned long long rip, unsigned long long acc)
{
    char buf[128];
    char *p = buf;
    size_t sl = strlen(tag);
    memcpy(p, tag, sl); p += sl;
    memcpy(p, " code=", 6); p += 6;
    veh_hex(&p, code);
    memcpy(p, " rip=", 5); p += 5;
    veh_hex(&p, rip);
    memcpy(p, " acc=", 5); p += 5;
    veh_hex(&p, acc);
    *p++ = '\r'; *p++ = '\n';
    veh_write(buf, (unsigned)(p - buf));
}

static void veh_dump_ring(const char *why)
{
    size_t sl = strlen(why);
    veh_write(why, (unsigned)sl);
    {
        long n = veh_ring_cnt, base;
        long i;
        if (n > 32) n = 32;
        base = (veh_ring_next - n + 32) % 32;
        for (i = 0; i < n; i++) {
            veh_ent_t *e = &veh_ring[(base + i) % 32];
            veh_line("[VEHR]", e->code, e->rip, e->acc);
        }
        veh_line("[VEHR]", 0, (unsigned long long)n, 0);
    }
}

/* B-2015: системные исключения, которые надо ГЛОТАТЬ (см. trap.c):
 * MSVC thread-name 0x406D1388 + OutputDebugString 0x40010006/0x4001000A.
 * ВАЖНО: клиентское exception-событие DR на них НЕ вызывается —
 * instrument.c: "Note: not called for RaiseException" (проверено по тегу
 * cronbuild-11.91.20763). Единственная точка до app-SEH — наш VEH:
 * он стоит первым в цепочке и видит ВСЕ исключения, включая
 * software-поднятые. EXCEPTION_CONTINUE_EXECUTION с НЕТРОНУТЫМ контекстом
 * = продолжение с ContextRecord->Rip, куда RtlRaiseException кладёт адрес
 * возврата из RaiseException — эквивалент тихого __except(EXECUTE_HANDLER)
 * макроса SetThreadName. Под DR <=v0.2.4 эти коды уходили в unhandled →
 * DR завершал процесс (Godot 4.5: NtTerminateProcess(self, 0x406D1388)). */
static int exc_swallow(unsigned int code)
{
    return code == 0x406D1388u || code == 0x40010006u || code == 0x4001000Au;
}

static long NTAPI veh_log(PEXCEPTION_POINTERS ep)
{
    if (ep == NULL || ep->ExceptionRecord == NULL ||
        ep->ContextRecord == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    unsigned int code = (unsigned int)ep->ExceptionRecord->ExceptionCode;

    /* B-2015: глотание ДО любого лога/кольца — тихо и дёшево.
     * Драйверы/Vulkan поднимают thread-name на КАЖДОМ новом потоке:
     * без раннего выхода лог бы распухал. */
    if (exc_swallow(code))
        return EXCEPTION_CONTINUE_EXECUTION;

    unsigned long long rip = (unsigned long long)ep->ContextRecord->Rip;
    unsigned long long acc = 0;
    int is_av = (code == 0xC0000005);
    if (is_av && ep->ExceptionRecord->NumberParameters >= 2)
        acc = (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1];

    /* (1) кольцо: только AV и #UD */
    if (is_av || code == 0xC000001D) {
        long slot = veh_ring_next;
        veh_ring[slot].rip  = rip;
        veh_ring[slot].acc  = acc;
        veh_ring[slot].code = code;
        veh_ring_next = (slot + 1) % 32;
        if (veh_ring_cnt < 32) veh_ring_cnt++;
    }

    /* (2) фатальный паттерн: AV внутри dynamorio.dll по near-NULL */
    if (is_av && veh_dr_lo != NULL && rip >= (unsigned long long)(uintptr_t)veh_dr_lo &&
        rip < (unsigned long long)(uintptr_t)veh_dr_hi && acc < 0x10000 &&
        veh_dumped == 0) {
        veh_dumped = 1;
        veh_dump_ring("[VEH-FATAL] near-NULL AV inside dynamorio.dll — ring of last AVs/#UD:\r\n");
    }

    /* (3) прежний лог: rip в клиентском модуле, бюджет 24 строки */
    if (rip >= (unsigned long long)(uintptr_t)veh_client_lo &&
        rip < (unsigned long long)(uintptr_t)veh_client_hi &&
        veh_budget < 24) {
        veh_budget++;
        veh_line("[VEH]", code, rip - (unsigned long long)(uintptr_t)veh_client_lo, acc);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void pass_veh_install(void *client_base, size_t client_size)
{
    veh_client_lo = client_base;
    veh_client_hi = (char *)client_base + client_size;
    HMODULE drm = GetModuleHandleA("dynamorio.dll");
    if (drm != NULL) {
        veh_dr_lo = (void *)drm;
        /* размер не критичен: верхнюю границу берём с запасом 64МБ */
        veh_dr_hi = (char *)drm + 0x4000000;
    }
    veh_errh = GetStdHandle(STD_ERROR_HANDLE);
    if (veh_errh == INVALID_HANDLE_VALUE || veh_errh == NULL) {
        /* stderr может быть не открыт — пишем хоть куда-то? нет: без дампа */
        veh_errh = NULL;
    }
    AddVectoredExceptionHandler(1, veh_log);
    {
        char msg[160];
        char *p = msg;
        memcpy(p, "[P.A.S.S.] VEH installed: client ", 33); p += 33;
        veh_hex(&p, (unsigned long long)(uintptr_t)veh_client_lo);
        memcpy(p, "..", 2); p += 2;
        veh_hex(&p, (unsigned long long)(uintptr_t)veh_client_hi);
        memcpy(p, " dr=", 4); p += 4;
        veh_hex(&p, (unsigned long long)(uintptr_t)veh_dr_lo);
        *p++ = '\r'; *p++ = '\n';
        veh_write(msg, (unsigned)(p - msg));
    }
}
#endif
