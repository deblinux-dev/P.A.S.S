/* P.A.S.S. — strace.c
 * B-2010 diag: трассировка syscall'ов приложения через
 * dr_register_pre/post_syscall_event — видимость, которой нет у
 * qemu -strace за инъекцией DR (qemu strace обрывается на execve).
 * Включается клиентской опцией -strace (getenv в инжектируемом
 * процессе ненадёжен — см. pass.c).
 * B-2013: содержит и ПОСТОЯННЫЙ фикс close_range (не diag!). */
#include "pass.h"
#ifdef LINUX
#include <sys/syscall.h> /* SYS_close_range; MSVC не знает этот заголовок */
#endif

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
#ifdef LINUX
#ifdef SYS_close_range
#    define PASS_SYS_CLOSE_RANGE SYS_close_range
#else
#    define PASS_SYS_CLOSE_RANGE 436 /* x86_64 */
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#    define CLOSE_RANGE_CLOEXEC 4
#endif
#endif /* LINUX */

static bool sc_filter(void *drcontext, int sysnum)
{
    (void)drcontext;
    /* B-2013: фикс-путь должен получать pre-событие всегда (LINUX:
     * нумерация сисколлов ядра), трейс-путь — только под -strace.
     * На Windows номера NT-сисколлов зависят от билда — фикс там
     * не зарегистрирован вовсе. */
    return g_strace
#ifdef LINUX
           || sysnum == PASS_SYS_CLOSE_RANGE
#endif
        ;
}

/* B-2013: pre-хук срабатывает ДО core-обработки DR (dispatch.c:
 * instrument_pre_syscall → return false = сисколл не исполняется
 * вовсе, 4G-цикл эмуляции не запускается). LINUX-only. */
#ifdef LINUX
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
#endif /* LINUX */

#ifdef WINDOWS
/* B-2015: имена NT-сисколлов (нумерация Win10 19041..22H2, таблица
 * j00ru/windows-syscalls x64/csv/nt.csv, колонка Windows 10 (20H2);
 * у пользователя bun сообщает "Windows v.win10_fe" = 19042 — совпадает).
 * На других билдах номера могут отличаться — имя печатается как подсказка. */
typedef struct { int num; const char *name; } ntname_t;
static const ntname_t g_nt_names[] = {
    { 5,   "NtCallbackReturn" },
    { 7,   "NtDeviceIoControlFile" },
    { 15,  "NtClose" },
    { 24,  "NtAllocateVirtualMemory" },
    { 25,  "NtQueryInformationProcess" },
    { 28,  "NtSetInformationProcess" },
    { 30,  "NtFreeVirtualMemory" },
    { 35,  "NtQueryVirtualMemory" },
    { 37,  "NtQueryInformationThread" },
    { 40,  "NtMapViewOfSection" },
    { 42,  "NtUnmapViewOfSection" },
    { 44,  "NtTerminateProcess" },
    { 55,  "NtOpenSection" },
    { 58,  "NtWriteVirtualMemory" },
    { 60,  "NtDuplicateObject" },
    { 67,  "NtContinue" },
    { 74,  "NtCreateSection" },
    { 80,  "NtProtectVirtualMemory" },
    { 83,  "NtTerminateThread" },
    { 118, "NtAllocateVirtualMemoryEx" },
    { 157, "NtGetContextThread" },
    { 193, "NtCreateThreadEx" },
    { 200, "NtCreateUserProcess" },
    { 414, "NtSetInformationVirtualMemory" },
    { 460, "NtUnmapViewOfSectionEx" },
};
static const char *nt_name(int sysnum)
{
    unsigned k;
    for (k = 0; k < sizeof(g_nt_names)/sizeof(g_nt_names[0]); k++)
        if (g_nt_names[k].num == sysnum)
            return g_nt_names[k].name;
    return NULL;
}
/* аллокационное семейство: post-лог NTSTATUS обязателен для диагноза
 * «memory allocation of N bytes failed» (Zig page_allocator = VirtualAlloc) */
static bool nt_status_interesting(int sysnum)
{
    switch (sysnum) {
    case 24: case 25: case 28: case 30: case 35: case 40: case 42: case 44:
    case 74: case 80: case 118: case 193: case 200: case 414: case 460:
        return true;
    }
    return false;
}

/* B-2015: коды, которые нативно глотает raiser (__except-обёртка MSVC
 * SetThreadName-макроса) и которые НЕ должны уводить процесс в смерть.
 * Общий список для всех уровней перехвата. */
static bool exc_code_swallow(unsigned code)
{
    return code == 0x406D1388u /* MSVC thread-name: драйверы NVIDIA/AMD/
                                  Intel, Vulkan-слой на каждом новом потоке */ ||
           code == 0x40010006u /* DBG_PRINTEXCEPTION_C (OutputDebugStringA) */ ||
           code == 0x4001000Au /* DBG_PRINTEXCEPTION_WIDE_C (ODS W, Win10+) */;
}

/* Последний рубеж: NtTerminateProcess(self, код) — точка необратимости.
 * На Server 2022/2025 RaiseException(0x406D1388) вообще НЕ доходит до
 * NtRaiseException: трасса показывает DebugPort-проверку →
 * NtSetInformationProcess(0x3F) → image-queries → NtTerminateProcess(self,
 * 0x406D1388) без единого raise-syscall и без доставки (CI-трейс 2c4fdb0).
 * Для кодов-глотаторов фейкуем SUCCESS: механизм «необработанного»
 * завершения продолжает работу и переразводит исключение через
 * RtlRaiseStatus — уже с реальной доставкой, которую глотает наш VEH
 * (EXCEPTION_CONTINUE_EXECUTION). На Win10 19042 (пользователь) raise
 * происходит и доставляется штатно — там работает VEH-уровень. */
static bool terminate_swallow(void *drcontext)
{
    ptr_int_t h = dr_syscall_get_param(drcontext, 0);
    ptr_int_t st = dr_syscall_get_param(drcontext, 1);
    if (h != -1 /* NtCurrentProcess */ && h != 0)
        return false;
    if (!exc_code_swallow((unsigned)(st & 0xffffffff)))
        return false;
    if (g_n_raise_swallowed < 50)
        dr_fprintf(STDERR,
                   "[P.A.S.S.] swallow NtTerminateProcess(self, 0x%08x) — "
                   "thread-name/ODS не должен убивать процесс\n",
                   (unsigned)(st & 0xffffffff));
    g_n_raise_swallowed++;
    dr_syscall_set_result(drcontext, 0); /* STATUS_SUCCESS */
    return true;
}
#endif /* WINDOWS */

/* B-2015: глотание системных исключений НА УРОВНЕ PRE-SYSCALL NtRaiseException.
 * Хронология попыток:
 *  (1) drmgr exception event — НЕ стреляет: instrument.c "Note: not called
 *      for RaiseException" (подтверждено по тегу cronbuild-11.91.20763);
 *  (2) наш VEH (первый в цепочке, EXCEPTION_CONTINUE_EXECUTION) — тоже
 *      мимо: DR не доводит software-исключения до app-диспетчерии (CI:
 *      tname умирал с ExitStatus 0x406D1388, ни [EXC], ни VEH-следа);
 *  (3) ЭТОТ путь — единственный, до которого DR нас допускает: pre-syscall
 *      событие (в DR 11.91 фильтр мёртв — события стреляют на КАЖДЫЙ
 *      syscall, чем успешно пользуется и close_range-фикс на Linux).
 * Для кодов, которые нативно глотает raiser (__except-обёртка MSVC-макроса
 * SetThreadName — их поднимают драйверы NVIDIA/AMD/Intel и Vulkan-слой на
 * каждом новом потоке) фейкуем STATUS_SUCCESS: RtlRaiseException видит
 * успех и просто возвращается — приложение продолжает ПОСЛЕ вызова
 * RaiseException, ровно как при тихом проглатывании. Нативная Windows не
 * глотает unhandled 0x406D1388 (CI: native rc=136 = код&0xFF) — под DR
 * до v0.2.4 процесс умирал с этим кодом (Godot 4.5 «окно появилось и
 * закрылось», crash.txt: NtTerminateProcess(self, 0x406D1388)).
 * C++-исключения (0xE06D7363) и прочие коды НЕ трогаются.
 * Номер NtRaiseException плавает по билдам (Win10 1507..1909: 332–352,
 * 2004..21H1: 358, 21H2+: 360, Win11: 365–372 — j00ru CSV), поэтому
 * проверяем ДИАПАЗОН 330..380: ложное попадание безопасно — arg0 чужого
 * syscall читается на 4 байта (dr_safe_read), не-наш код → обычное
 * исполнение без всяких модификаций. */
#define PASS_SYS_RAISE_LO 330
#define PASS_SYS_RAISE_HI 380
int g_n_raise_swallowed; /* не static: печатается в итогах pass.c (Windows) */
#ifdef WINDOWS
static bool nt_raise_swallow(void *drcontext, ptr_int_t excrec)
{
    unsigned code = 0;
    if (excrec == 0)
        return false;
    /* EXCEPTION_RECORD.ExceptionCode — DWORD по смещению 0 */
    if (!dr_safe_read((void *)excrec, sizeof(code), &code, NULL))
        return false;
    if (code != 0x406D1388u /* MSVC thread-name */ &&
        code != 0x40010006u /* DBG_PRINTEXCEPTION_C (OutputDebugStringA) */ &&
        code != 0x4001000Au /* DBG_PRINTEXCEPTION_WIDE_C (ODS W, Win10+) */)
        return false;
    /* B-2015: печать ВСЕГДА (не verbose-гейт) — это ЖИЗНЕННО важные события
     * (без них процесс умирает); кап 50 строк — thread-name бывает пачками. */
    if (g_n_raise_swallowed < 50)
        dr_fprintf(STDERR,
                   "[P.A.S.S.] swallow RaiseException 0x%08x "
                   "(thread-name/ODS — нативно глотается raiser'ом)\n", code);
    g_n_raise_swallowed++;
    dr_syscall_set_result(drcontext, 0); /* STATUS_SUCCESS */
    return true;
}
#else
/* Linux: переменная нужна только для счётчика — держим единый символ */
#endif /* WINDOWS */

static bool sc_pre(void *drcontext, int sysnum)
{
    ptr_int_t a0, a1, a2;
#ifdef LINUX
    a0 = dr_syscall_get_param(drcontext, 0);
    a1 = dr_syscall_get_param(drcontext, 1);
    if (sysnum == PASS_SYS_CLOSE_RANGE &&
        a1 - a0 > PASS_CLOSE_RANGE_LIMIT)
        return true; /* уже обработан sc_pre_fix_close_range */
#endif
#ifdef WINDOWS
    /* ДО гейта трейса: глотание работает и без -strace (постоянный фикс,
     * как close_range на Linux). false = syscall не исполнять. */
    if (sysnum >= PASS_SYS_RAISE_LO && sysnum <= PASS_SYS_RAISE_HI &&
        nt_raise_swallow(drcontext, dr_syscall_get_param(drcontext, 0)))
        return false;
    if (sysnum == 44 /* NtTerminateProcess */ && terminate_swallow(drcontext))
        return false;
#endif
    /* B-2015: СВОЙ гейт трейса. Ядро DR 11.91.20763 НЕ вызывает
     * instrument_filter_syscall вовсе (0 call sites в core: dispatch.c /
     * kernel.c / os.c — регрессия этого cronbuild-тега), поэтому фильтр
     * sc_filter мёртв и pre/post-события приходят для КАЖДОГО syscall.
     * Без этого гейта клиент печатал [SC]-поток и без -strace (у пользователя
     * на Windows так и было) + держал каждый syscall в clean call с
     * dr_fprintf — потоки убивались внутри clean call'ов с DR-локами →
     * дедлок экзита (HANG bun -e на CI §7.3, killed NtTerminateProcess(143)
     * не завершал процесс). Гейт здесь обязателен. */
    if (!g_strace)
        return true;
    a0 = dr_syscall_get_param(drcontext, 0);
    a1 = dr_syscall_get_param(drcontext, 1);
    a2 = dr_syscall_get_param(drcontext, 2);
#ifdef WINDOWS
    /* B-2015: write/execve-блоки ниже — Linux-нумерация (1/20/59):
     * на NT-номерах они дали бы ложную печать (59 = NtReadVirtualMemory). */
    {
        const char *nm = nt_name(sysnum);
        if (nm != NULL)
            dr_fprintf(STDERR, "[SC %d %s] %p %p %p\n", sysnum, nm, a0, a1, a2);
        else
            dr_fprintf(STDERR, "[SC %d] %p %p %p\n", sysnum, a0, a1, a2);
    }
    return true;
#else
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
    /* B-2014: execve — печатаем путь и argv (spawn-циклы Godot/bun) */
    if (sysnum == 59) {
        char pbuf[128];
        if (a0 && dr_safe_read((void *)a0, sizeof(pbuf), pbuf, NULL)) {
            pbuf[sizeof(pbuf) - 1] = 0;
            dr_fprintf(STDERR, "[SC 59 execve] \"%s\" argv:", pbuf);
            char *av[8];
            dr_safe_read((void *)a1, sizeof(av), av, NULL);
            for (int k = 0; k < 8 && av[k]; k++) {
                char ab[96];
                if (dr_safe_read(av[k], sizeof(ab), ab, NULL)) {
                    ab[sizeof(ab) - 1] = 0;
                    dr_fprintf(STDERR, " [%s]", ab);
                }
            }
            dr_fprintf(STDERR, "\n");
        }
    }
    /* компактный поток прочих syscall'ов */
    dr_fprintf(STDERR, "[SC %d] %p %p %p\n", sysnum, a0, a1, a2);
    return true;
#endif /* WINDOWS/Linux-разводка печати */
}

static void sc_post(void *drcontext, int sysnum)
{
    ptr_int_t res;
    if (!g_strace)
        return; /* гейт см. sc_pre: фильтр DR 11.91.20763 мёртв */
    res = dr_syscall_get_result(drcontext);
#ifdef LINUX
    if (sysnum == 1 || sysnum == 20 || sysnum == 231 /*exit_group*/ ||
        sysnum == 232 /*exit*/ || sysnum == 60)
        dr_fprintf(STDERR, "[SC %d ->] %p\n", sysnum, res);
#else
    /* B-2015: NTSTATUS аллокационного семейства — диагноз Zig-паники
     * «memory allocation of N bytes failed»: видно, КАКОЙ VirtualAlloc
     * и С КАКИМ статусом отказал (0xC0000017 = NO_MEMORY и т.п.). */
    if (nt_status_interesting(sysnum))
        dr_fprintf(STDERR, "[SC %d ->] %p (%s)\n", sysnum, res,
                   (res == 0) ? "STATUS_SUCCESS" : "FAIL");
#endif
}

void pass_strace_init(void)
{
    drmgr_register_filter_syscall_event(sc_filter);
#ifdef LINUX
    drmgr_register_pre_syscall_event(sc_pre_fix_close_range);
#endif
    drmgr_register_pre_syscall_event(sc_pre);
    drmgr_register_post_syscall_event(sc_post);
}
