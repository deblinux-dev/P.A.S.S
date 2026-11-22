/* P.A.S.S. — strace.c
 * B-2010 diag: трассировка syscall'ов приложения через
 * dr_register_pre/post_syscall_event — видимость, которой нет у
 * qemu -strace за инъекцией DR (qemu strace обрывается на execve).
 * Включается клиентской опцией -strace (getenv в инжектируемом
 * процессе ненадёжен — см. pass.c).
 * B-2013: содержит и ПОСТОЯННЫЙ фикс close_range (не diag!). */
#include "pass.h"
#include <sys/syscall.h>

/* B-2013: обход дефекта DR 11.91.20763: SYS_close_range эмулируется
 * циклом по КАЖДОМУ fd диапазона (os.c: for (i=first; i<=last; i++)
 * → fd_is_dr_owned(i) → TABLE_RWLOCK(fd_table) на каждой итерации).
 * Zig-стд (bun 1.3.14+) зовёт close_range(4, 0xffffffff,
 * CLOSE_RANGE_CLOEXEC) при старте → ~4.3 млрд итераций = вечный
 * спин 100% CPU без единого сисколла (подтверждено на чистом DR
 * без клиента; сэмплер: d_r_read_lock на rwlock fd_table, w=
 * DR_WHERE_SYSCALL_HANDLER). Диапазоны длиннее лимита пропускаем
 * с fake-success:
 *  - CLOSE_RANGE_CLOEXEC ничего не закрывает — безопасный no-op;
 *  - real-close огромного диапазона — задокументированное
 *    отклонение (fds остаются открытыми); альтернатива — спин DR
 *    на часы. Малые диапазоны уходят в честную эмуляцию DR.       */
#define PASS_CLOSE_RANGE_LIMIT 4096
#ifdef SYS_close_range
#    define PASS_SYS_CLOSE_RANGE SYS_close_range
#else
#    define PASS_SYS_CLOSE_RANGE 436 /* x86_64 */
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#    define CLOSE_RANGE_CLOEXEC 4
#endif

static bool sc_filter(void *drcontext, int sysnum)
{
    (void)drcontext;
    /* B-2013: фикс-путь должен получать pre-событие всегда,
     * трейс-путь — только под -strace */
    return g_strace || sysnum == PASS_SYS_CLOSE_RANGE;
}

/* B-2013: pre-хук срабатывает ДО core-обработки DR (dispatch.c:
 * instrument_pre_syscall → return false = сисколл не исполняется
 * вовсе, 4G-цикл эмуляции не запускается). */
static bool sc_pre_fix_close_range(void *drcontext, int sysnum)
{
    ptr_int_t first, last, flags;
    if (sysnum != PASS_SYS_CLOSE_RANGE)
        return true;
    first = dr_syscall_get_param(drcontext, 0);
    last = dr_syscall_get_param(drcontext, 1);
    flags = dr_syscall_get_param(drcontext, 2);
    if (last - first <= PASS_CLOSE_RANGE_LIMIT)
        return true; /* малый диапазон — честная эмуляция DR */
    dr_fprintf(STDERR,
               "[P.A.S.S.] close_range(%ld..%ld, flags=%ld): диапазон > %d —"
               " fake-success, обход 4G-цикла эмуляции DR (B-2013)%s\n",
               (long)first, (long)last, (long)flags, PASS_CLOSE_RANGE_LIMIT,
               (flags & CLOSE_RANGE_CLOEXEC) ? " [CLOEXEC no-op]" : "");
    dr_syscall_set_result(drcontext, 0);
    return false; /* не исполнять сисколл и не входить в эмуляцию DR */
}

static bool sc_pre(void *drcontext, int sysnum)
{
    ptr_int_t a0 = dr_syscall_get_param(drcontext, 0);
    ptr_int_t a1 = dr_syscall_get_param(drcontext, 1);
    ptr_int_t a2 = dr_syscall_get_param(drcontext, 2);
    if (sysnum == PASS_SYS_CLOSE_RANGE &&
        a1 - a0 > PASS_CLOSE_RANGE_LIMIT)
        return true; /* уже обработан sc_pre_fix_close_range */
    if (sysnum == 1 /*write*/ || sysnum == 20 /*writev*/) {
        dr_fprintf(STDERR, "[SC %d] %p %p %p", sysnum, a0, a1, a2);
        if (sysnum == 1 && (a0 == 1 || a0 == 2) && a1 && a2 && a2 <= 128) {
            uint8_t tmp[128];
            size_t n = (size_t)a2;
            if (dr_safe_read((void *)a1, n, tmp, NULL)) {
                dr_fprintf(STDERR, " \"");
                for (size_t i = 0; i < n; i++) {
                    uint8_t c = tmp[i];
                    if (c == '\n')      dr_fprintf(STDERR, "\\n");
                    else if (c == '\t') dr_fprintf(STDERR, "\\t");
                    else if (c >= 0x20 && c < 0x7f) dr_fprintf(STDERR, "%c", c);
                    else                dr_fprintf(STDERR, "\\x%02x", c);
                }
                dr_fprintf(STDERR, "\"");
            } else {
                dr_fprintf(STDERR, " <unreadable>");
            }
        }
        dr_fprintf(STDERR, "\n");
        return true;
    }
    /* компактный поток прочих syscall'ов */
    dr_fprintf(STDERR, "[SC %d] %p %p %p\n", sysnum, a0, a1, a2);
    return true;
}

static void sc_post(void *drcontext, int sysnum)
{
    ptr_int_t res = dr_syscall_get_result(drcontext);
    if (sysnum == 1 || sysnum == 20 || sysnum == 231 /*exit_group*/ ||
        sysnum == 232 /*exit*/ || sysnum == 60)
        dr_fprintf(STDERR, "[SC %d ->] %p\n", sysnum, res);
}

void pass_strace_init(void)
{
    drmgr_register_filter_syscall_event(sc_filter);
    drmgr_register_pre_syscall_event(sc_pre_fix_close_range);
    drmgr_register_pre_syscall_event(sc_pre);
    drmgr_register_post_syscall_event(sc_post);
}
