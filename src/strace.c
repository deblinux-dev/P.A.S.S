/* P.A.S.S. — strace.c
 * B-2010 diag: трассировка syscall'ов приложения через
 * dr_register_pre/post_syscall_event — видимость, которой нет у
 * qemu -strace за инъекцией DR (qemu strace обрывается на execve).
 * Включается клиентской опцией -strace (getenv в инжектируемом
 * процессе ненадёжен — см. pass.c).                               */
#include "pass.h"

static bool sc_filter(void *drcontext, int sysnum)
{
    (void)drcontext;
    return g_strace;
}

static bool sc_pre(void *drcontext, int sysnum)
{
    ptr_int_t a0 = dr_syscall_get_param(drcontext, 0);
    ptr_int_t a1 = dr_syscall_get_param(drcontext, 1);
    ptr_int_t a2 = dr_syscall_get_param(drcontext, 2);
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
    drmgr_register_pre_syscall_event(sc_pre);
    drmgr_register_post_syscall_event(sc_post);
}
