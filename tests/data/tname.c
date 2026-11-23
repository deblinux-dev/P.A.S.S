/* P.A.S.S. — tname.c: регресс-тест B-2015 (Godot 4.5 «окно появилось и закрылось»)
 * RaiseException(0x406D1388) БЕЗ __try-обёртки — форма, в которой thread-name
 * исключение поднимают компоненты, рассчитанные на тихое игнорирование
 * (и нативно не падающие: NVIDIA/AMD/Intel-драйверы, Vulkan-слой — при старте
 * потоков во время инициализации графики, когда окно уже появилось).
 *
 * Под DR 11.91 код 0x406D1388 уходил в unhandled → DR завершал процесс
 * с ExitStatus 0x406D1388 (натурный репрод: Godot_v4.5-stable_win64.exe
 * под pass.exe -target penryn, crash.txt пользователя, NtTerminateProcess
 * (self, 0x406D1388) последней строкой).
 *
 * Фикс B-2015: клиент глотает системные исключения (0x406D1388 /
 * 0x40010006 / 0x4001000A) — dr_redirect_execution с доставленного RIP
 * (= адрес возврата из RaiseException), эквивалент тихого продолжения.
 * Ожидаемо под P.A.S.S.: печать "TNAME-OK", rc=0.                    */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    /* классическая THREADNAME_INFO из MSVC-макроса SetThreadName */
    ULONG_PTR info[4];
    info[0] = 0x1000;                                  /* dwType */
    info[1] = (ULONG_PTR)GetCurrentThreadId();         /* dwThreadID */
    info[2] = (ULONG_PTR)"pass-tname-main";            /* szName */
    info[3] = 0;                                       /* dwFlags */
    RaiseException(0x406D1388, 0, 4, info);
    /* сюда доходим только если исключение проглочено */
    puts("TNAME-OK");
    return 0;
}
