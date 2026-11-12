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

/* ════════ Windows: VEH-лог исключений в клиентском модуле ════════
 * §7.3: фатальный фолт при bun происходит в КОДЕ КЛИЕНТА (RVA
 * 0x245eb), DR классифицирует его как internal crash и исключение
 * до dr_exception_event не доходит. VEH с первым приоритетом видит
 * КАЖДОЕ исключение до DR-хендлера; логируем только те, чей rip
 * внутри клиентского модуля (остальные — служебные DR, их тысячи). */
#ifdef WINDOWS
#include <windows.h>

static void *veh_client_lo, *veh_client_hi;

static long NTAPI veh_log(PEXCEPTION_POINTERS ep)
{
    if (ep == NULL || ep->ExceptionRecord == NULL ||
        ep->ContextRecord == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t rip = (uintptr_t)ep->ContextRecord->Rip;
    if (rip < (uintptr_t)veh_client_lo || rip >= (uintptr_t)veh_client_hi)
        return EXCEPTION_CONTINUE_SEARCH;
    dr_fprintf(STDERR, "[VEH] code=0x%08x rip-rva=" PIFX " access=" PIFX
               " rsp=" PIFX "\n",
               (unsigned)ep->ExceptionRecord->ExceptionCode,
               rip - (uintptr_t)veh_client_lo,
               (ep->ExceptionRecord->ExceptionCode == 0xC0000005 &&
                ep->ExceptionRecord->NumberParameters >= 2)
                   ? (ptr_uint_t)ep->ExceptionRecord->ExceptionInformation[1]
                   : 0,
               (ptr_uint_t)ep->ContextRecord->Rsp);
    return EXCEPTION_CONTINUE_SEARCH;
}

void pass_veh_install(void *client_base, size_t client_size)
{
    veh_client_lo = client_base;
    veh_client_hi = (char *)client_base + client_size;
    AddVectoredExceptionHandler(1, veh_log);
    dr_fprintf(STDERR, "[P.A.S.S.] VEH installed: client " PIFX ".." PIFX "\n",
               (ptr_uint_t)veh_client_lo, (ptr_uint_t)veh_client_hi);
}
#endif
