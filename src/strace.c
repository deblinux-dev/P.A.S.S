/* P.A.S.S. — strace.c
 * B-2010 diag: трассировка syscall'ов приложения через
 * dr_register_pre/post_syscall_event — видимость, которой нет у
 * qemu -strace за инъекцией DR (qemu strace обрывается на execve).
 * Включается клиентской опцией -strace (getenv в инжектируемом
 * процессе ненадёжен — см. pass.c).
 * B-2013: содержит и ПОСТОЯННЫЙ фикс close_range (не diag!). */
#include "pass.h"
#include "drwrap.h" /* B-2018: зонд IsProcessorFeaturePresent (только Windows-вызовы) */
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
/* счётчик проглоченных thread-name/ODS исключений (печатается в итогах
 * pass.c); не static — extern из pass.c (Windows)
 */
int g_n_raise_swallowed;

/* B-2020: дамп кольца бит-опов на поимке OOM + захват коммита 0x10000
 * (pass_b2020.c) */
extern void pass_b2020_oom_dump(void);
extern void pass_b2020_note_commit(uint64_t base, uint64_t size);
static ptr_int_t g_b2_last_sc24_sz = 0;   /* запрошенный size последнего SC 24 */

/* B-2015: имена NT-сисколлов (нумерация Win10 19041..22H2, таблица
 * j00ru/windows-syscalls x64/csv/nt.csv, колонка Windows 10 (20H2);
 * у пользователя bun сообщает "Windows v.win10_fe" = 19042 — совпадает).
 * На других билдах номера могут отличаться — имя печатается как подсказка. */
typedef struct { int num; const char *name; } ntname_t;
static const ntname_t g_nt_names[] = {
    { 4,   "NtWaitForSingleObject" },
    { 5,   "NtCallbackReturn" },
    { 6,   "NtReadFile" },
    { 7,   "NtDeviceIoControlFile" },
    { 8,   "NtWriteFile" },      /* B-2018: НЕ 54! см. разбор OOM-детектора */
    { 15,  "NtClose" },
    { 18,  "NtOpenKey" },        /* реестр: (PHANDLE, DesiredAccess, OBJATTR) */
    { 23,  "NtQueryValueKey" },
    { 24,  "NtAllocateVirtualMemory" },
    { 25,  "NtQueryInformationProcess" },
    { 28,  "NtSetInformationProcess" },
    { 30,  "NtFreeVirtualMemory" },
    { 35,  "NtQueryVirtualMemory" },
    { 37,  "NtQueryInformationThread" },
    { 40,  "NtMapViewOfSection" },
    { 42,  "NtUnmapViewOfSection" },
    { 44,  "NtTerminateProcess" },
    { 54,  "NtQuerySystemInformation" }, /* B-2018: 0x36=54 — НЕ NtWriteFile */
    { 55,  "NtOpenSection" },
    { 58,  "NtWriteVirtualMemory" },
    { 60,  "NtDuplicateObject" },
    { 67,  "NtContinue" },
    { 73,  "NtQueryVolumeInformationFile" },
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

/* ── B-2018: скан стека на адреса возврата — печать модуль+RVA ──
 * fastfail (0xC0000409) недостижим, но в окне записи паники
 * (NtWriteFile с текстом) и NtTerminateProcess(self) стек ещё жив:
 * кадры от точки смерти до входа в панику видны как кандидаты адресов
 * возврата. RVA бун-фреймов позже символизируются офлайн (по .pdata,
 * как в разборе дампов B-2016). Шумные совпадения (остатки на стеке)
 * идут вперемешку с настоящими кадрами — это приемлемо: дубли
 * подряд гасятся, лимит 40 строк. */
void pass_diag_stack_scan(void *dc)
{
    dr_mcontext_t mc;
    int shown = 0;
    app_pc prev = NULL;
    memset(&mc, 0, sizeof(mc));
    mc.size = sizeof(mc);
    mc.flags = DR_MC_INTEGER | DR_MC_CONTROL;
    if (dc == NULL || !dr_get_mcontext(dc, &mc))
        return;
    dr_fprintf(STDERR, "[P.A.S.S.][STACK] rsp=" PIFX " — кандидаты адресов возврата:\n",
               (ptr_uint_t)mc.xsp);
    for (size_t off = 0; off < 0x4000 && shown < 40; off += 8) {
        app_pc val = NULL;
        module_data_t *md;
        const char *nm;
        if (!dr_safe_read((void *)(mc.xsp + off), sizeof(val), &val, NULL))
            continue;
        if (val == prev || (ptr_uint_t)val < 0x10000)
            continue;
        md = dr_lookup_module(val);
        if (md == NULL)
            continue;
        nm = dr_module_preferred_name(md);
        dr_fprintf(STDERR, "  [+0x%04x] %s+0x%06x\n",
                   (unsigned)off, nm != NULL ? nm : "?",
                   (unsigned)(val - md->start));
        shown++;
        prev = val;
        dr_free_module_data(md);
    }
}

/* ── B-2018: паттерны паники Zig-аллокатора «memory allocation of …» ──
 * B-2017-ФИКС (баг ASCII-ветки): раньше проверялись 'a','l','l' по
 * смещениям +16..18 — но в строке «memory allocation of …» там стоят
 * 'n',' ','o' («allocatio n…»), т.е. ASCII-ветка НЕ МОГЛА сработать
 * НИКОГДА. Верно: 'a' в +7 (после «memory '), 'l','l' в +8,+9. */
static bool mem_ascii_hit(const uint8_t *t, size_t i)
{
    return t[i + 0] == 'm' && t[i + 1] == 'e' && t[i + 2] == 'm' &&
           t[i + 3] == 'o' && t[i + 4] == 'r' && t[i + 5] == 'y' &&
           t[i + 6] == ' ' && t[i + 7] == 'a' && t[i + 8] == 'l';
}

/* UTF-16LE: m\0e\0m\0o\0r\0y\0 \0a\0l\0 (WriteConsoleW-путь condrv) */
static bool mem_utf16_hit(const uint8_t *t, size_t i)
{
    return t[i + 0] == 'm' && t[i + 1] == 0 && t[i + 2] == 'e' &&
           t[i + 3] == 0 && t[i + 4] == 'm' && t[i + 5] == 0 &&
           t[i + 6] == 'o' && t[i + 7] == 0 && t[i + 8] == 'r' &&
           t[i + 9] == 0 && t[i + 10] == 'y' && t[i + 11] == 0 &&
           t[i + 12] == ' ' && t[i + 13] == 0 && t[i + 14] == 'a' &&
           t[i + 15] == 0 && t[i + 16] == 'l' && t[i + 17] == 0;
}

static void oom_print_chunk(const uint8_t *t, size_t n, bool utf16)
{
    dr_fprintf(STDERR, "[P.A.S.S.][OOM] запись: \"");
    if (!utf16) {
        for (size_t k = 0; k < n && k < 64; k++)
            dr_fprintf(STDERR, "%c", (t[k] >= 0x20 && t[k] < 0x7f) ? t[k] : '.');
    } else {
        for (size_t k = 0; k + 1 < n && k / 2 < 64; k += 2)
            dr_fprintf(STDERR, "%c", (t[k] >= 0x20 && t[k] < 0x7f) ? t[k] : '.');
    }
    dr_fprintf(STDERR, "\"\n");
}

/* ── B-2018: зонд IsProcessorFeaturePresent ──
 * Часть рантаймов спрашивает фичи CPU НЕ через CPUID, а через
 * IsProcessorFeaturePresent — она читает KUSER_SHARED_DATA, заполненный
 * ядром по НАСТОЯЩЕМУ CPU при загрузке: спуф CPUID сюда не доходит.
 * Кандидат на объяснение «CPU lacks AVX support» у baseline-bun 1.2.14
 * (наш CPUID-спуф согласован — OSXSAVE/XSAVE/leaf-D выставляются
 * вместе с AVX, XCR0=0x7; значит запрос шёл МИМО CPUID).
 * Под -strace логируем пары (код → нативный ответ); спуф добавим
 * отдельным шагом, когда увидим, какие коды и что о них думает ОС.
 * Документированные коды: 6=SSE, 10=SSE2, 13=SSE3, 17=XSAVE_ENABLED,
 * 14=CMPXCHG16B; SSE4.2/POPCNT/AVX — недокументированные (20+). */
static int      g_ifp_codes[32];
static unsigned g_ifp_n;

static void ifp_pre(void *wrapc, void **user_data)
{
    /* drwrap_get_arg гарантирован только в pre — захватываем arg0
     * (код фичи) в user_data для post-колбэка */
    *user_data = (void *)(ptr_int_t)drwrap_get_arg(wrapc, 0);
}

static void ifp_post(void *wrapc, void *user_data)
{
    /* drwrap передаёт в post ЗНАЧЕНИЕ, записанное pre в *user_data
     * (код фичи, сохранённый как указатель) — НЕ указатель на слот.
     * Прежняя pre-сигнатура (void**) была и типо-ошибкой (gcc 14 на
     * mingw-раннере: incompatible-pointer-types = error, до этого
     * молча предупреждение), и рантайм-багом: *user_data читала
     * память по адресу 6..17 (код фичи как указатель).            */
    ptr_int_t code = (ptr_int_t)(uintptr_t)user_data;
    ptr_int_t res;
    (void)wrapc;
    if (!g_strace)
        return;
    res  = (ptr_int_t)drwrap_get_retval(wrapc);
    for (unsigned k = 0; k < g_ifp_n; k++)
        if (g_ifp_codes[k] == (int)code)
            return;
    if (g_ifp_n < 32) {
        g_ifp_codes[g_ifp_n++] = (int)code;
        dr_fprintf(STDERR,
                   "[P.A.S.S.][IFP] IsProcessorFeaturePresent(%d) -> %d"
                   " (нативный ответ; KUSER_SHARED_DATA не спуфится CPUID-хуком)\n",
                   (int)code, (int)res);
    }
}

void pass_ifp_probe_install(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE kbase = GetModuleHandleA("kernelbase.dll");
    void *a1 = k32 ? (void *)GetProcAddress(k32, "IsProcessorFeaturePresent") : NULL;
    void *a2 = kbase ? (void *)GetProcAddress(kbase, "IsProcessorFeaturePresent") : NULL;
    int n = 0;
    extern int g_no_ifp; /* B-2025: -noifp (бисекция краша drwrap) */
    if (g_no_ifp) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.] IFP-зонд SKIPPED (-noifp, бисекция B-2025)\n");
        return;
    }
    if (!pass_drwrap_ensure())
        return;
    if (a1 != NULL && drwrap_wrap(a1, ifp_pre, ifp_post))
        n++;
    if (a2 != NULL && a2 != a1 && drwrap_wrap(a2, ifp_pre, ifp_post))
        n++;
    if (n > 0)
        dr_fprintf(STDERR, "[P.A.S.S.] IFP-зонд установлен x%d (B-2018)\n", n);
}

/* ── B-2030: спуф IsProcessorFeaturePresent (продолжение B-2018) ──
 * KUSER_SHARED_DATA заполнен ядром по НАСТОЯЩЕМУ CPU: CPUID-спуф туда
 * не доходит. Доказано живьём (Penryn): под PASS CPUID видит sse42=1,
 * а IsProcessorFeaturePresent(38)=FALSE — из-за этого лаунчер Blender
 * 4.2 показывает диалог «требуется процессор с SSE4.2» и уходит в отказ.
 * Схема: drwrap_wrap (не replace!) — оригинал отвечает нативно, post
 * ДОБАВЛЯЕТ TRUE для фич, которые PASS эмулирует (exposed_features()).
 * Только добавляем: нативные ответы для немапированных кодов (NX,
 * SLAT, RDWRFSGSBASE…) сохраняются. Коды PF: 13=SSE3, 36=SSSE3,
 * 37=SSE4.1, 38=SSE4.2, 39=AVX, 40=AVX2, 41=AVX512F, 60=BMI2
 * (winnt.h 10.0.26100).                                       */
static unsigned g_ifp_spoof_n;

/* DBG: файловый лог вызовов IFP — включается env PASS_IFP_LOG=<путь>
 * (GUI-проценты вроде blender.exe сбрасывают std-хендлы, STDERR клиента
 * до редиректа не доходит — доказано живьём). */
static file_t g_ifp_logf = INVALID_FILE;

static void pass_ifp_log_open(void)
{
    char buf[MAX_PATH];
    DWORD n;
    if (g_ifp_logf != INVALID_FILE)
        return;
    n = GetEnvironmentVariableA("PASS_IFP_LOG", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        g_ifp_logf = dr_open_file(buf, DR_FILE_WRITE_OVERWRITE);
        if (g_ifp_logf != INVALID_FILE)
            dr_fprintf(g_ifp_logf, "IFP-log open (B-2030 dbg)\n");
    }
}

/* drwrap_replace (как B-2015 RaiseException): drwrap_wrap на kernel32
 * НЕ СТРЕЛЯЕТ (проверено живьём: pre-колбэк не вызывается, ответ OS
 * не меняется), а replace работает — RaiseException x3 доказан.
 * Честный ответ берём из ntdll!RtlIsProcessorFeaturePresent — оригинал
 * (kernel32/kernelbase заменены, прямого вызова в них нет → рекурсии
 * нет).                                                        */
static BOOLEAN (WINAPI *g_pfn_RtlIFP)(unsigned long);

static BOOLEAN WINAPI pass_IFP_repl(unsigned long code)
{
    BOOLEAN res = FALSE;
    feat_t f;
    if (g_pfn_RtlIFP != NULL)
        res = g_pfn_RtlIFP(code);
    if (res) {
        if (g_ifp_logf != INVALID_FILE)
            dr_fprintf(g_ifp_logf, "IFP(%lu) native=1\n", code);
        return res;
    }
    switch (code) {
    case 13: f = F_SSE3;  break;
    case 36: f = F_SSSE3; break;
    case 37: f = F_SSE41; break;
    case 38: f = F_SSE42; break;
    case 39: f = F_AVX;   break;
    case 40: f = F_AVX2;  break;
    case 60: f = F_BMI2;  break;
    default:
        if (g_ifp_logf != INVALID_FILE)
            dr_fprintf(g_ifp_logf, "IFP(%lu) native=0 unmapped\n", code);
        return res;  /* немапированный код — нативный ответ */
    }
    if (!has(exposed_features(), f)) {
        if (g_ifp_logf != INVALID_FILE)
            dr_fprintf(g_ifp_logf, "IFP(%lu) native=0 not-exposed\n", code);
        return res; /* PASS это не заявляет — консистентно с CPUID-спуфом */
    }
    if (g_ifp_logf != INVALID_FILE)
        dr_fprintf(g_ifp_logf, "IFP(%lu) native=0 spoofed=1\n", code);
    if (g_ifp_spoof_n < 30) {
        g_ifp_spoof_n++;
        dr_fprintf(STDERR,
                   "[P.A.S.S.][IFP] spoof IFP(%lu) -> TRUE (exposed, B-2030)\n",
                   code);
    }
    return TRUE;
}

void pass_ifp_spoof_install(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE kbase = GetModuleHandleA("kernelbase.dll");
    HMODULE ntd = GetModuleHandleA("ntdll.dll");
    module_data_t *k32md;
    void *a1, *a2, *a3;
    int n = 0;
    pass_ifp_log_open();
    if (ntd == NULL)
        return;
    g_pfn_RtlIFP = (BOOLEAN (WINAPI *)(unsigned long))
        (void *)GetProcAddress(ntd, "RtlIsProcessorFeaturePresent");
    if (g_pfn_RtlIFP == NULL) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.] IFP-spoof SKIPPED: нет ntdll!RtlIsProcessor"
                   "FeaturePresent\n");
        return;
    }
    if (!pass_drwrap_ensure())
        return;
    a1 = k32 ? (void *)GetProcAddress(k32, "IsProcessorFeaturePresent") : NULL;
    a2 = kbase ? (void *)GetProcAddress(kbase, "IsProcessorFeaturePresent") : NULL;
    k32md = dr_lookup_module_by_name("KERNEL32.DLL");
    a3 = (k32md != NULL) ? (void *)dr_get_proc_address(k32md->handle,
                                                       "IsProcessorFeaturePresent")
                         : NULL;
    if (k32md != NULL)
        dr_free_module_data(k32md);
    if (a1 != NULL && drwrap_replace(a1, (void *)pass_IFP_repl, false))
        n++;
    if (a2 != NULL && a2 != a1 && drwrap_replace(a2, (void *)pass_IFP_repl, false))
        n++;
    if (a3 != NULL && a3 != a1 && a3 != a2 &&
        drwrap_replace(a3, (void *)pass_IFP_repl, false))
        n++;
    dr_fprintf(STDERR,
               "[P.A.S.S.] IFP-spoof замен x%d (B-2030: PF38 SSE4.2/PF39 AVX/"
               "PF40 AVX2/PF60 BMI2 добавляются по exposed_features)\n", n);
    if (g_ifp_logf != INVALID_FILE)
        dr_fprintf(g_ifp_logf,
                   "install: k32=%p kbase=%p dr3=%p replaces=%d\n",
                   a1, a2, a3, n);
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

/* B-2015, ЗАМЕТКА: NtTerminateProcess(self, код) — точка необратимости,
 * но fake-success там даёт БЕСКОНЕЧНЫЙ ЦИКЛ (fake → ntdll переразводит
 * через RtlRaiseStatus → шорт-кат → terminate → fake...; CI: 3+ итерации,
 * re-raise никогда не доходит до доставки). Уровень ВЫКЛЮЧЕН — активный
 * перехват перенесён В ИСТОЧНИК: drwrap-замена kernel32!RaiseException
 * (trap.c: pass_raise_hook_install) — для кодов-глотаторов тихий выход
 * до ЛЮБОЙ OS-механики.                                      */
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
    /* ── B-2016/B-2017/B-2018: OOM-детектор ──
     * Zig-аллокатор bun паникует «memory allocation of N bytes failed»
     * и сразу abort() → __fastfail(7) → 0xC0000409, который ОБХОДИТ все
     * user-mode обработчики (exception event не стреляет, итоги не
     * печатаются). Пишемое сообщение — последняя точка, где мы ещё живы.
     *
     * B-2018, ГЛАВНЫЙ ФИКС: сообщение паники bun пишет через
     * NtWriteFile, а он на Win10/11 x64 = SC 8 (j00ru 20H2: 0x08),
     * НЕ 54! SC 54 = NtQuerySystemInformation — его param5/6 мусорны,
     * детектор B-2016/B-2017 сканировал НЕ ТОТ сисколл и молчал.
     * Натурный лог B-2017 подтверждает: куски сообщения перемешаны
     * ровно с [SC 8] 0x58 (NtWriteFile на консольный хэндл).
     * NtDeviceIoControlFile (SC 7, condrv-ioctl) оставлен как второй
     * источник (WriteConsoleW-путь). Карта параметров SC 7 из B-2017:
     * inbuf=param6/insize=param7 + outbuf=param8/outsize=param9;
     * у NtWriteFile буфер=param5/длина=param6 (0-based N-й аргумент).
     * Срабатывание → стек-скан + полный дамп (кольцо generic-опов+PC,
     * счётчики, регистры) — независимо от -strace.
     * Плюс B-2017-фикс ASCII-паттерна: 'a','l','l' должны проверяться
     * в +7/+8/+9 («memory al…»), а не в +16..18 (там «n o»).
     * Окно скана 256→512 (в condrv-сообщении текст может лежать
     * за пределами первых 256 байт). */
    if (sysnum == 8 || sysnum == 7) {
        static volatile long oom_dumps;
        ptr_int_t b[2], l[2];
        if (sysnum == 8) {
            /* NtWriteFile: buffer=param5, len=param6 */
            b[0] = dr_syscall_get_param(drcontext, 5);
            l[0] = dr_syscall_get_param(drcontext, 6);
            b[1] = 0; l[1] = 0;
        } else {
            /* NtDeviceIoControlFile: ioctl=param5, inbuf=param6,
             * insize=param7, outbuf=param8, outsize=param9 */
            b[0] = dr_syscall_get_param(drcontext, 6);
            l[0] = dr_syscall_get_param(drcontext, 7);
            b[1] = dr_syscall_get_param(drcontext, 8);
            l[1] = dr_syscall_get_param(drcontext, 9);
        }
        for (int q = 0; q < 2; q++) {
            uint8_t tmp[512];
            size_t n;
            if (b[q] == 0) continue;
            n = (l[q] > 0 && l[q] < (ptr_int_t)sizeof(tmp))
                    ? (size_t)l[q] : sizeof(tmp);
            if (!dr_safe_read((void *)b[q], n, tmp, NULL)) continue;
            for (size_t i = 0; i + 18 <= n; i++) {
                bool hit = false;
                bool utf16 = false;
                if (mem_ascii_hit(tmp, i)) {
                    hit = true;
                } else if (mem_utf16_hit(tmp, i)) {
                    hit = true;
                    utf16 = true;
                }
                if (hit) {
                    if (oom_dumps < 3) {
                        oom_dumps++;
                        /* размер из сообщения — «memory allocation of
                         * <десятичное> bytes failed»; это запрошенный
                         * объём, который аллокатор НЕ смог выдать
                         * (у пользователя детерминированный 5085 ≈
                         * env-блок в UTF-8) */
                        unsigned long long sz = 0;
                        size_t j = i + (utf16 ? 42 : 21);
                        bool any = false;
                        while (j + (utf16 ? 1 : 0) < n &&
                               tmp[j] >= '0' && tmp[j] <= '9') {
                            sz = sz * 10 + (unsigned long long)(tmp[j] - '0');
                            any = true;
                            j += utf16 ? 2 : 1;
                        }
                        dr_fprintf(STDERR,
                            "[P.A.S.S.][OOM] паника 'memory allocation of N bytes"
                            " failed' поймана В ЗАПИСИ (fastfail обходит"
                            " обработчики — это последнее окно; SC %d, %s):\n",
                            sysnum, utf16 ? "UTF-16LE" : "ASCII");
                        oom_print_chunk(tmp + i, n - i, utf16);
                        if (any)
                            dr_fprintf(STDERR,
                                "[P.A.S.S.][OOM] запрошенный размер: %llu\n", sz);
                        pass_b2020_oom_dump();   /* B-2020: кольцо бит-опов + дизасм-байты */
                        pass_diag_stack_scan(drcontext);
                        pass_diag_dump("OOM-abort", drcontext);
                    }
                    break;
                }
            }
        }
    }
    /* ── B-2018: дамп на NtTerminateProcess(self) ──
     * Уровень необратимости: всё, что выходит через ExitProcess/Zig
     * std.process.exit/RtlExitUserProcess, доходит сюда ПОСЛЕДНИМ.
     * Дамп (стек-скан + кольцо + регистры) печатается и при НОРМАЛЬНОМ
     * выходе (шум в 5 строк), и при смерти с кодом — у пользователя
     * это второе гарантированное окно рядом с OOM-детектором. */
    if (sysnum == 44) {
        static volatile long term_dumps;
        ptr_int_t h = dr_syscall_get_param(drcontext, 0);
        ptr_int_t code = dr_syscall_get_param(drcontext, 1);
        if ((h == -1 || h == -2) && term_dumps < 2) {
            term_dumps++;
            dr_fprintf(STDERR,
                       "[P.A.S.S.][EXIT] NtTerminateProcess(self, %p) —"
                       " дамп перед смертью:\n", (void *)code);
            pass_diag_stack_scan(drcontext);
            pass_diag_dump("terminate", drcontext);
        }
    }
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
    /* B-2018: расшифровка аллокационной семьи — дерефы размеров/защиты.
     * Без этого по [SC 118/24] не видно, ЧТО резервирует mimalloc
     * (сегменты 64МБ? коммиты page-map?) — а это ключевая улика
     * для «page-map lookup → NULL» из разбора дампов B-2016. */
    if (sysnum == 24 || sysnum == 118 || sysnum == 30 || sysnum == 80) {
        ptr_int_t p1 = dr_syscall_get_param(drcontext, 1);
        ptr_int_t p2 = dr_syscall_get_param(drcontext, 2);
        ptr_int_t p3 = dr_syscall_get_param(drcontext, 3);
        ptr_int_t p4 = dr_syscall_get_param(drcontext, 4);
        ptr_int_t p5 = dr_syscall_get_param(drcontext, 5);
        ptr_int_t sz = 0, base = 0;
        if (p1 >= 0x10000)
            (void)dr_safe_read((void *)p1, sizeof(base), &base, NULL);
        if (sysnum == 30) {
            /* NtFreeVirtualMemory(P, *BaseAddress, *RegionSize, FreeType) */
            if (p2 >= 0x10000)
                (void)dr_safe_read((void *)p2, sizeof(sz), &sz, NULL);
            dr_fprintf(STDERR, "[SC %d ·] base=%p sz=%p type=%p\n",
                       sysnum, (void *)base, (void *)sz,
                       (void *)(p3 & 0xffffffff));
        } else if (sysnum == 118) {
            /* B-2019: NtAllocateVirtualMemoryEx на Win10 19041+/Win11 —
             * сигнатура БЕЗ ZeroBits: (P, *Base, *RegionSize, Type,
             * Protect[, ExtParams, Count]). Лог B-2018: r9 (param2) —
             * УКАЗАТЕЛЬ на RegionSize, а param3 = Type (число —
             * дerefa проваливалась и печатала 0). Печатаем: base=*p1,
             * size=*p2, type=p3, prot=p4 (младшие dword — старшие
             * половины слотов — мусор соседних данных). */
            if (p2 >= 0x10000)
                (void)dr_safe_read((void *)p2, sizeof(sz), &sz, NULL);
            dr_fprintf(STDERR,
                       "[SC %d ·] base=%p size=%p type=%p prot=%p\n",
                       sysnum, (void *)base, (void *)sz,
                       (void *)(p3 & 0xffffffff), (void *)(p4 & 0xffffffff));
        } else {
            /* NtAllocateVirtualMemory(P, *BaseAddress, ZeroBits,
             * *RegionSize, Type, Protect) и
             * NtProtectVirtualMemory(P, *BaseAddress, *RegionSize,
             * NewProtect, *OldProtect) */
            if (p3 >= 0x10000)
                (void)dr_safe_read((void *)p3, sizeof(sz), &sz, NULL);
            if (sysnum == 80) {
                /* B-2020: у NtProtectVirtualMemory p3 = NewProtect (ЧИСЛО),
                 * p4 = *OldProtect (УКАЗАТЕЛЬ — у B-2019 он печатался как
                 * prot, что и давало «кодовый адрес» 0x7ff6529eb750).
                 * Старшие dword слотов stack-аргументов протухшие
                 * (32-битная запись не чистит верх qword) — маскируем,
                 * как в SC 118. */
                dr_fprintf(STDERR,
                           "[SC %d ·] base=%p size=%p newprot=%p oldprot_ptr=%p\n",
                           sysnum, (void *)base, (void *)sz,
                           (void *)(p3 & 0xffffffff), (void *)p4);
            } else {
                dr_fprintf(STDERR,
                           "[SC %d ·] base=%p size=%p type=%p prot=%p\n",
                           sysnum, (void *)base, (void *)sz,
                           (void *)(p4 & 0xffffffff), (void *)(p5 & 0xffffffff));
            }
            if (sysnum == 24)
                g_b2_last_sc24_sz = sz;   /* для захвата коммита в sc_post */
        }
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
    /* B-2019: фактическая база результата аллокационного семейства.
     * Для SC 24/118 — куда встал резерв/коммит (у B-2018 было видно
     * только ЗАПРОШЕННОЕ base, но 1GB-резерв шёл с base=0 = выбор ОС,
     * и где он приземлился — из лога не следовало); для SC 30 — что
     * именно разкоммитили; для SC 80 — старая защита (*OldProtect). */
    if (res == 0 &&
        (sysnum == 24 || sysnum == 118 || sysnum == 30 || sysnum == 80)) {
        ptr_int_t p1 = dr_syscall_get_param(drcontext, 1);
        ptr_int_t out = 0;
        if (p1 >= 0x10000 &&
            dr_safe_read((void *)p1, sizeof(out), &out, NULL) && out != 0)
            dr_fprintf(STDERR, "[SC %d retbase] %p\n", sysnum, (void *)out);
        /* B-2020: коммит страницы сегмента (запрошенный size 0x10000,
         * база вида segbase+0x…0000) — адрес для seg-форензики на
n         * отказе segget (pass_b2020.c) */
        if (sysnum == 24 && g_b2_last_sc24_sz == 0x10000 && out != 0)
            pass_b2020_note_commit((uint64_t)out, (uint64_t)g_b2_last_sc24_sz);
    }
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
#ifdef WINDOWS
    /* B-2030: спуф IsProcessorFeaturePresent — ПО УМОЛЧАНИЮ (KUSER_SHARED_
     * DATA не спуфится CPUID-хуком; без этого лаунчер Blender и другие
     * IFP-проверки видят настоящий CPU). Гейт -noifpspoof. */
    if (!g_no_ifpspoof)
        pass_ifp_spoof_install();
    /* B-2018: зонд IsProcessorFeaturePresent — данные для решения о
     * спуфе KUSER_SHARED_DATA (кандидат на «CPU lacks AVX support»
     * у baseline-bun). Под -strace: хуки не должны трогать живой
     * дефолтный путь без диагностической задачи. */
    if (g_strace)
        pass_ifp_probe_install();
#endif
}
