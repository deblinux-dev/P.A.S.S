/* ============================================================
 * P.A.S.S. — pass.exe (Windows-лаунчер) v0.2.1
 * ------------------------------------------------------------
 * Простой запуск программ под P.A.S.S.:
 *
 *     pass.exe [ключи] -- program.exe [аргументы...]
 *
 *   -target <имя>     профиль CPU: core2 (по умолчанию),
 *                     nehalem, westmere, sandybridge, haswell, native
 *   -verbose <N>      диагностика 0..4
 *   -root <каталог>   каталог DynamoRIO (по умолчанию — автопоиск)
 *   -no-download      не скачивать DynamoRIO автоматически
 *   прочие -ключи     передаются клиенту P.A.S.S. как есть
 *
 * Лаунчер ищет ПОЛНУЮ установку DynamoRIO (bin64\drrun.exe +
 * lib64\release\dynamorio.dll — «огрызок» из одного drrun.exe не
 * принимается: без lib64 DR не может инжектиться) в:
 *   1) каталоге pass.exe (и в подпапках dynamorio\,
 *      DynamoRIO-Windows-11.91.20763\, DynamoRIO-*\);
 *   2) %DYNAMORIO_HOME%;
 *   3) %LOCALAPPDATA%\PASS\dynamorio (сюда же авто-скачивание).
 * Найденный корень передаётся drrun ЯВНО через -root: сам drrun,
 * запущенный вне bin64\, вычисляет корень на два уровня вверх и
 * получает «incomplete installation» (баг пользователя v0.2.0).
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

static void join2(char *out, size_t cap, const char *a, const char *b)
{
    _snprintf(out, cap - 1, "%s\\%s", a, b);
    out[cap - 1] = 0;
}

/* корень DynamoRIO валиден: есть bin64\drrun.exe (или drrun.exe в
 * самом корне — раскладка «распаковали содержимое»), lib64 и клиент
 * dynamorio.dll. Возвращает путь к drrun.exe. */
static bool dr_ok(const char *root, char *out, size_t cap)
{
    char p[MAX_PATH * 2];
    join2(p, sizeof(p), root, "bin64\\drrun.exe");
    if (!file_exists(p)) {
        join2(p, sizeof(p), root, "drrun.exe");
        if (!file_exists(p))
            return false;
    }
    join2(p, sizeof(p), root, "lib64\\release\\dynamorio.dll");
    if (!file_exists(p))
        return false;
    join2(p, sizeof(p), root, "bin64\\drrun.exe");
    if (file_exists(p)) {
        strncpy(out, p, cap - 1);
        out[cap - 1] = 0;
        return true;
    }
    join2(p, sizeof(p), root, "drrun.exe");
    strncpy(out, p, cap - 1);
    out[cap - 1] = 0;
    return true;
}

static bool try_root(const char *root, char *drrun, size_t cap)
{
    char p[MAX_PATH];
    if (root == NULL || root[0] == 0)
        return false;
    if (dr_ok(root, drrun, cap))
        return true;
    (void)p;
    return false;
}

/* поиск полной установки; found_root — корень для drrun -root */
static bool find_drrun(char *drrun, size_t dcap, char *root, size_t rcap)
{
    char dir[MAX_PATH], cand[MAX_PATH * 2], pattern[MAX_PATH * 2];
    exe_dir(dir, sizeof(dir));

    /* 1) сам каталог pass.exe */
    if (try_root(dir, drrun, dcap)) {
        strncpy(root, dir, rcap - 1); root[rcap - 1] = 0;
        return true;
    }
    /* 2) рядом: dynamorio\ и DynamoRIO-Windows-<версия>\ */
    join2(cand, sizeof(cand), dir, "dynamorio");
    if (try_root(cand, drrun, dcap)) {
        strncpy(root, cand, rcap - 1); root[rcap - 1] = 0;
        return true;
    }
    join2(cand, sizeof(cand), dir, "DynamoRIO-Windows-" DR_VERSION);
    if (try_root(cand, drrun, dcap)) {
        strncpy(root, cand, rcap - 1); root[rcap - 1] = 0;
        return true;
    }
    /* 3) любые DynamoRIO-* рядом (распакованный релиз любой версии) */
    {
        HANDLE h;
        WIN32_FIND_DATAA fd;
        join2(pattern, sizeof(pattern), dir, "DynamoRIO-*");
        h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    join2(cand, sizeof(cand), dir, fd.cFileName);
                    if (try_root(cand, drrun, dcap)) {
                        strncpy(root, cand, rcap - 1); root[rcap - 1] = 0;
                        FindClose(h);
                        return true;
                    }
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    /* 4) DYNAMORIO_HOME */
    {
        const char *home = getenv("DYNAMORIO_HOME");
        if (home != NULL && try_root(home, drrun, dcap)) {
            strncpy(root, home, rcap - 1); root[rcap - 1] = 0;
            return true;
        }
    }
    /* 5) %LOCALAPPDATA%\PASS\dynamorio (цель авто-скачивания) */
    {
        const char *lad = getenv("LOCALAPPDATA");
        if (lad != NULL) {
            join2(cand, sizeof(cand), lad, "PASS\\dynamorio");
            if (try_root(cand, drrun, dcap)) {
                strncpy(root, cand, rcap - 1); root[rcap - 1] = 0;
                return true;
            }
        }
    }
    return false;
}

/* авто-скачивание полной установки в %LOCALAPPDATA%\PASS\dynamorio */
static bool download_dr(char *drrun, size_t dcap, char *root, size_t rcap)
{
    const char *lad = getenv("LOCALAPPDATA");
    char cmd[8192], cand[MAX_PATH * 2];
    int rc;
    if (lad == NULL)
        return false;
    print_utf8("pass.exe: DynamoRIO не найден — скачиваю релиз " DR_VERSION
               " (~450 МБ, один раз)...\n");
    _snprintf(cmd, sizeof(cmd) - 1,
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop';"
        "[Net.ServicePointManager]::SecurityProtocol="
        "[Net.SecurityProtocolType]::Tls12;"
        "$d='%s\\PASS';"
        "New-Item -ItemType Directory -Force -Path $d | Out-Null;"
        "$z=Join-Path $d 'dr.zip'; $u=Join-Path $d 'unz';"
        "Invoke-WebRequest -Uri '%s' -OutFile $z;"
        "New-Item -ItemType Directory -Force -Path $u | Out-Null;"
        "if (Get-Command tar -ErrorAction SilentlyContinue)"
        " { tar -xf $z -C $u }"
        " else { Expand-Archive -Force -Path $z -DestinationPath $u };"
        "$src=(Get-ChildItem $u -Directory | Select-Object -First 1).FullName;"
        "Move-Item -Force $src (Join-Path $d 'dynamorio');"
        "Remove-Item -Force $z; Remove-Item -Force -Recurse $u\"",
        lad, DR_URL);
    cmd[sizeof(cmd) - 1] = 0;
    rc = system(cmd);
    if (rc != 0)
        fail("не удалось скачать/распаковать DynamoRIO (нужен интернет).\n"
             "  Ручная установка: распакуйте DynamoRIO-Windows-" DR_VERSION
             ".zip рядом с pass.exe\n"
             "  (папка DynamoRIO-Windows-" DR_VERSION " целиком — вместе с"
             " lib64\\) и повторите запуск.");
    join2(cand, sizeof(cand), lad, "PASS\\dynamorio");
    if (try_root(cand, drrun, dcap)) {
        strncpy(root, cand, rcap - 1); root[rcap - 1] = 0;
        return true;
    }
    return false;
}

int main(int argc, char **argv)
{
    char drrun[MAX_PATH * 2], client[MAX_PATH * 2], dir[MAX_PATH];
    char cmdline[32768], root[MAX_PATH * 2];
    char user_root[MAX_PATH * 2] = "";
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
        } else if (strcmp(argv[i], "-root") == 0 && i + 1 < argc) {
            strncpy(user_root, argv[++i], sizeof(user_root) - 1);
            user_root[sizeof(user_root) - 1] = 0;
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
                "  -root <каталог> каталог DynamoRIO (автопоиск по умолчанию)\n"
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

    if (user_root[0] != 0) {
        /* явный -root: валидировать и использовать */
        if (!try_root(user_root, drrun, sizeof(drrun)))
            fail("в -root нет полной установки DynamoRIO: нужен\n"
                 "  <root>\\bin64\\drrun.exe и <root>\\lib64\\release\\dynamorio.dll\n"
                 "  (распакуйте релиз DynamoRIO ЦЕЛИКОМ).");
        strncpy(root, user_root, sizeof(root) - 1);
        root[sizeof(root) - 1] = 0;
    } else if (!find_drrun(drrun, sizeof(drrun), root, sizeof(root))) {
        if (!allow_download)
            fail("DynamoRIO не найден (и -no-download задан): положите\n"
                 "  распакованный релиз DynamoRIO рядом с pass.exe или задайте"
                 " -root / DYNAMORIO_HOME.");
        if (!download_dr(drrun, sizeof(drrun), root, sizeof(root)))
            fail("DynamoRIO не найден и скачать не удалось: положите\n"
                 "  распакованный релиз DynamoRIO рядом с pass.exe"
                 " (папка целиком) или задайте -root.");
    }

    /* командная строка: drrun -root <корень> -c "pass_client.dll"
     * [опции клиента] -- app args...
     * -root передаётся ЯВНО: drrun вне bin64\ иначе вычисляет корень
     * на два уровня вверх (incomplete installation, v0.2.0).       */
    len = (size_t)_snprintf(cmdline, sizeof(cmdline) - 1,
        "\"%s\" -root \"%s\" -c \"%s\" ", drrun, root, client);
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
        fail("не удалось запустить drrun (см. PASS_LAUNCHER_DEBUG=1 для командной строки)");
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code))
        code = 1;
    CloseHandle(pi.hProcess);
    return (int)code;
}
