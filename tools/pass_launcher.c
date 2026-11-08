/* ============================================================
 * P.A.S.S. — pass.exe (Windows-лаунчер)
 * ------------------------------------------------------------
 * Простой запуск программ под P.A.S.S.:
 *
 *     pass.exe [ключи] -- program.exe [аргументы...]
 *
 *   -target <имя>     профиль CPU: core2 (по умолчанию),
 *                     nehalem, westmere, sandybridge, haswell, native
 *   -verbose <N>      диагностика 0..4
 *   -no-download      не скачивать DynamoRIO автоматически
 *   прочие -ключи     передаются клиенту P.A.S.S. как есть
 *
 * Лаунчер сам находит drrun.exe (рядом с собой, в bin64\, в
 * %DYNAMORIO_HOME% или %LOCALAPPDATA%\PASS\dynamorio) и при
 * отсутствии скачивает релиз DynamoRIO с GitHub.
 *
 * Сборка (MSVC):  cl /O2 /Fe:pass.exe pass_launcher.c
 * Сборка (MinGW): gcc -O2 -o pass.exe pass_launcher.c
 * ============================================================ */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define DR_VERSION "11.91.20763"
#define DR_URL "https://github.com/DynamoRIO/dynamorio/releases/download/" \
               "cronbuild-" DR_VERSION "/DynamoRIO-Windows-" DR_VERSION ".zip"

static void print_utf8(const char *s)
{
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, (DWORD)strlen(s),
              &written, NULL);
}

static void fail(const char *msg)
{
    print_utf8("pass.exe: ошибка: ");
    print_utf8(msg);
    print_utf8("\n");
    exit(2);
}

/* каталог, где лежит pass.exe */
static void exe_dir(char *out, size_t cap)
{
    char full[MAX_PATH];
    char *slash;
    GetModuleFileNameA(NULL, full, MAX_PATH);
    slash = strrchr(full, '\\');
    if (slash != NULL)
        *slash = 0;
    strncpy(out, full, cap - 1);
    out[cap - 1] = 0;
}

static bool file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* найти drrun.exe; при need=true и отсутствии — скачать */
static void find_drrun(char *out, size_t cap, bool allow_download)
{
    char dir[MAX_PATH], path[MAX_PATH * 2];
    exe_dir(dir, sizeof(dir));

    /* 1) рядом с pass.exe и в bin64\ */
    _snprintf(path, sizeof(path) - 1, "%s\\drrun.exe", dir);
    path[sizeof(path) - 1] = 0;
    if (file_exists(path)) { strncpy(out, path, cap - 1); out[cap - 1] = 0; return; }

    _snprintf(path, sizeof(path) - 1, "%s\\bin64\\drrun.exe", dir);
    path[sizeof(path) - 1] = 0;
    if (file_exists(path)) { strncpy(out, path, cap - 1); out[cap - 1] = 0; return; }

    /* 2) DYNAMORIO_HOME */
    {
        const char *home = getenv("DYNAMORIO_HOME");
        if (home != NULL) {
            _snprintf(path, sizeof(path) - 1, "%s\\bin64\\drrun.exe", home);
            path[sizeof(path) - 1] = 0;
            if (file_exists(path)) { strncpy(out, path, cap - 1); out[cap - 1] = 0; return; }
        }
    }

    /* 3) %LOCALAPPDATA%\PASS\dynamorio\bin64\drrun.exe */
    {
        const char *lad = getenv("LOCALAPPDATA");
        if (lad != NULL) {
            _snprintf(path, sizeof(path) - 1,
                      "%s\\PASS\\dynamorio\\bin64\\drrun.exe", lad);
            path[sizeof(path) - 1] = 0;
            if (file_exists(path)) { strncpy(out, path, cap - 1); out[cap - 1] = 0; return; }
            if (!allow_download)
                fail("drrun.exe не найден (и -no-download задан)");
            print_utf8("pass.exe: DynamoRIO не найден — скачиваю релиз " DR_VERSION "...\n");
            /* mkdir + download + unzip через PowerShell */
            _snprintf(path, sizeof(path) - 1,
                "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
                "$ErrorActionPreference='Stop';"
                "$d='%s\\PASS';"
                "New-Item -ItemType Directory -Force -Path $d | Out-Null;"
                "Invoke-WebRequest -Uri '%s' -OutFile '$d\\dr.zip';"
                "Expand-Archive -Force -Path '$d\\dr.zip' -DestinationPath '$d\\unz';"
                "$src=Get-ChildItem '$d\\unz' -Directory | Select-Object -First 1;"
                "Move-Item -Force -Path $src.FullName -Destination '$d\\dynamorio';"
                "Remove-Item -Force '$d\\dr.zip'; Remove-Item -Force -Recurse '$d\\unz'\"",
                lad, DR_URL);
            path[sizeof(path) - 1] = 0;
            {
                int rc = system(path);
                if (rc != 0)
                    fail("не удалось скачать/распаковать DynamoRIO (нужен интернет "
                         "или ручная установка: положите drrun.exe рядом с pass.exe)");
            }
            _snprintf(path, sizeof(path) - 1,
                      "%s\\PASS\\dynamorio\\bin64\\drrun.exe", lad);
            path[sizeof(path) - 1] = 0;
            if (file_exists(path)) { strncpy(out, path, cap - 1); out[cap - 1] = 0; return; }
        }
    }
    fail("drrun.exe не найден: положите его рядом с pass.exe или задайте DYNAMORIO_HOME");
}

int main(int argc, char **argv)
{
    char drrun[MAX_PATH * 2], client[MAX_PATH * 2], dir[MAX_PATH];
    char cmdline[32768];
    const char *target = "core2";
    bool allow_download = true;
    int app_argc0 = argc;              /* индекс первого аргумента приложения */
    size_t len;
    int i;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;

    SetConsoleOutputCP(65001);   /* UTF-8 для русских сообщений */

    exe_dir(dir, sizeof(dir));
    _snprintf(client, sizeof(client) - 1, "%s\\pass_client.dll", dir);
    client[sizeof(client) - 1] = 0;

    /* разбор ключей лаунчера */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) { app_argc0 = i + 1; break; }
        if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "-no-download") == 0) {
            allow_download = false;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-help") == 0) {
            print_utf8(
                "P.A.S.S. — запуск программ с эмуляцией новых инструкций\n"
                "на старых CPU (по умолчанию профиль Core 2).\n\n"
                "  pass.exe [ключи] -- program.exe [args...]\n\n"
                "Ключи:\n"
                "  -target <имя>   core2 | nehalem | westmere | sandybridge | haswell | native\n"
                "  -verbose <N>    диагностика 0..4\n"
                "  -no-download    не скачивать DynamoRIO автоматически\n"
                "  -h              эта справка\n\n"
                "Остальные -ключи передаются клиенту P.A.S.S. без изменений.\n");
            return 0;
        }
        /* прочие ключи уходят клиенту (подставляются ниже) */
    }
    (void)target;   /* используется в цикле ниже как клиентская опция */
    if (app_argc0 >= argc)
        fail("не указана программа: pass.exe [ключи] -- program.exe [args]");
    if (!file_exists(client))
        fail("pass_client.dll не найден рядом с pass.exe");

    find_drrun(drrun, sizeof(drrun), allow_download);

    /* командная строка: drrun -c "pass_client.dll" [опции клиента] -- app args...
     * Форма -c <client> [opts...] до "--" корректно парсится drrun на обеих ОС. */
    len = (size_t)_snprintf(cmdline, sizeof(cmdline) - 1,
        "\"%s\" -c \"%s\" ", drrun, client);
    cmdline[sizeof(cmdline) - 1] = 0;

    /* клиентские опции: всё до --, включая -target/-verbose */
    for (i = 1; i < app_argc0 - 1; i++) {
        if (strcmp(argv[i], "--") == 0)
            break;
        len += (size_t)_snprintf(cmdline + len, sizeof(cmdline) - len - 1,
                                 "\"%s\" ", argv[i]);
        if (len >= sizeof(cmdline) - 2) break;
    }
    len += (size_t)_snprintf(cmdline + len, sizeof(cmdline) - len - 1, "-- ");
    for (i = app_argc0; i < argc; i++) {
        len += (size_t)_snprintf(cmdline + len, sizeof(cmdline) - len - 1,
                                 "\"%s\" ", argv[i]);
        if (len >= sizeof(cmdline) - 2) break;
    }

    if (getenv("PASS_LAUNCHER_DEBUG") != NULL) {
        print_utf8(cmdline);
        print_utf8("\n");
    }

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL,
                        &si, &pi))
        fail("не удалось запустить drrun (см. выше, как найти DynamoRIO)");
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code))
        code = 1;
    CloseHandle(pi.hProcess);
    return (int)code;
}
