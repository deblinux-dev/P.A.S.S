/* iso_client.c — изоляционный клиент B-2011: В ЛЮБОЙ базовый блок вставляется
 * один тривиальный clean call. Назначение: определить, пишет ли СТАБ DR
 * (pushf/подготовка аргументов) на СТЕК ПРИЛОЖЕНИЯ ниже rsp (в красную зону)
 * или DR полностью уводит clean call на свой dstack.
 * Если данные красной зоны листовой функции выживают — DR чист, и порча
 * t_redzone создаётся ТОЛЬКО собственными pushf-танцами нашего клиента.
 */
#include "dr_api.h"
#include "drmgr.h"

static volatile long sink;

static void cc(void) { sink++; }

static dr_emit_flags_t event_bb(void *drcontext, void *tag, instrlist_t *bb,
                                instr_t *inst, bool for_trace, bool translating,
                                void *user_data)
{
    if (translating)
        return DR_EMIT_DEFAULT;
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void *)cc,
                         false /*no fpstate*/, 0);
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmgr_init();
    drmgr_register_bb_instrumentation_event(NULL /*pre*/, event_bb, NULL);
}
