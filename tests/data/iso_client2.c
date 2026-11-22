/* iso_client2.c — изоляция v2: clean call С fp=true И 1 аргументом,
 * вставленный В СЕРЕДИНЕ BB (как generic-путь P.A.S.S.). v1 (fp=false,
 * 0 аргов, BB-start) красную зону не бил; проверяем флаги параметров. */
#include "dr_api.h"
#include "drmgr.h"

static volatile long sink;
static void cc(long x) { sink += x; }

static dr_emit_flags_t event_ins(void *drcontext, instrlist_t *bb,
                                 instr_t *inst, instr_t *inst_next,
                                 bool for_trace, bool translating, void *ud)
{
    if (translating || instr_is_meta(inst))
        return DR_EMIT_DEFAULT;
    dr_insert_clean_call(drcontext, bb, inst, (void *)cc, true /*fp!*/, 1,
                         OPND_CREATE_INTPTR(1));
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmgr_init();
    drmgr_register_bb_insertion_event(event_ins);
}
