/* P.A.S.S. — sampler.c (диагностика B-2009, LINUX only)
 * PC-сэмплинг через dr_set_itimer (ITIMER_PROF): колбэк периодически
 * печатает app-PC прерванного потока — так ловится user-mode спин
 * без gdbstub. Включается env-ом PASS_SAMPLER=<мс>; в проде нет.  */
#define _GNU_SOURCE
#include "pass.h"
#ifdef LINUX
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

static void sampler_cb(void *drcontext, dr_mcontext_t *mc)
{
    app_pc tag = NULL;
    void *wtag = NULL;
    dr_where_am_i_t w = dr_where_am_i(drcontext, mc->pc, &wtag);
    tag = (app_pc)wtag;
    dr_fprintf(STDERR,
               "[P.A.S.S.] sampler: pc=" PIFX " w=%d tag=" PIFX " rax=" PIFX
               " rbx=" PIFX " rcx=" PIFX " rdx=" PIFX " rsi=" PIFX " rdi=" PIFX
               " rbp=" PIFX " rsp=" PIFX " st=[",
               (ptr_uint_t)mc->pc, (int)w, (ptr_uint_t)tag,
               (ptr_uint_t)mc->xax, (ptr_uint_t)mc->xbx, (ptr_uint_t)mc->xcx,
               (ptr_uint_t)mc->xdx, (ptr_uint_t)mc->xsi, (ptr_uint_t)mc->xdi,
               (ptr_uint_t)mc->xbp, (ptr_uint_t)mc->xsp);
    for (int q = 0; q < 8; q++) {
        ptr_uint_t v = 0;
        if (dr_safe_read((void *)(mc->xsp + (ptr_uint_t)q * 8), sizeof(v),
                         &v, NULL))
            dr_fprintf(STDERR, "%s" PIFX, q ? "," : "", v);
        else
            dr_fprintf(STDERR, "%s?", q ? "," : "");
    }
    /* защита-прототипы dst/src: внутри ли они кэша DR? */
    app_pc base; size_t sz; uint prot; bool okw;
    okw = dr_query_memory((app_pc)mc->xax, &base, &sz, &prot);
    dr_fprintf(STDERR, "] dstprot=%d base=" PIFX " sz=" PIFX,
               okw ? (int)prot : -1,
               okw ? (ptr_uint_t)base : 0, okw ? (ptr_uint_t)sz : 0);
    okw = dr_query_memory((app_pc)mc->xsi, &base, &sz, &prot);
    dr_fprintf(STDERR, " srcprot=%d base=" PIFX " sz=" PIFX,
               okw ? (int)prot : -1,
               okw ? (ptr_uint_t)base : 0, okw ? (ptr_uint_t)sz : 0);
    /* что копируем: первые 32 байта src */
    uint8_t blk[32];
    if (dr_safe_read((void *)mc->xsi, sizeof(blk), blk, NULL)) {
        dr_fprintf(STDERR, " srcb=");
        for (int q = 0; q < 16; q++)
            dr_fprintf(STDERR, "%02x", blk[q]);
    }
    /* B-2013: rbx/rdi — дамп rwlock-полей (read_write_lock_t без DAA:
     * +0 lock_requests, +4 prev_owner, +8 num_readers, +0xc writer,
     * +0x10 num_pending_readers) и атрибуция объекта */
    {
        app_pc base2; size_t sz2; uint prot2; bool ok2;
        for (int r = 0; r < 2; r++) {
            ptr_uint_t obj = r ? mc->xbx : mc->xdi;
            if (obj == 0)
                continue;
            ok2 = dr_query_memory((app_pc)obj, &base2, &sz2, &prot2);
            dr_fprintf(STDERR, " %s=0x" PIFX " prot=%d base=" PIFX " sz=" PIFX,
                       r ? "rbxobj" : "rdiobj", obj, ok2 ? (int)prot2 : -1,
                       ok2 ? (ptr_uint_t)base2 : 0, ok2 ? (ptr_uint_t)sz2 : 0);
            if (ok2 && dr_safe_read((void *)obj, 20, blk, NULL)) {
                int *iv = (int *)blk;
                dr_fprintf(STDERR, " f=[%d %d %d w=%d p=%d]", iv[0], iv[1],
                           iv[2], iv[3], iv[4]);
            }
        }
    }
    dr_fprintf(STDERR, "\n");
}

void pass_sampler_start(long interval_ms)
{
    if (interval_ms < 20)
        interval_ms = 100;
    if (dr_set_itimer(ITIMER_PROF, (uint)interval_ms, sampler_cb))
        dr_fprintf(STDERR, "[P.A.S.S.] sampler: itimer ON every %ldms\n",
                   interval_ms);
    else
        dr_fprintf(STDERR, "[P.A.S.S.] sampler: itimer FAILED\n");
}
#endif /* LINUX */
