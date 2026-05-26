#include <windows.h>

#include <cstdio>


namespace camera_no_clip
{
    auto run() noexcept -> void;
}


namespace
{
    // Toggle to false to suppress the helper console.  Default true because
    // the hotkey-driven build benefits from seeing the [INFO]/[ERROR] lines
    // emitted by main.cpp; redirect them elsewhere if you ship this.
    inline constexpr bool use_console{ true };

    auto worker_thread(const HMODULE module_handle) noexcept
        -> DWORD
    {
        FILE* dummy{ nullptr };

        if constexpr (use_console)
        {
            FreeConsole();
            AllocConsole();
            (void)freopen_s(&dummy, "CONOUT$", "w", stdout);
            (void)freopen_s(&dummy, "CONOUT$", "w", stderr);
            SetConsoleTitleA("camera-no-clip");

            if (const HWND console_window{ GetConsoleWindow() })
            {
                ShowWindow(console_window, SW_SHOW);
            }
        }

        camera_no_clip::run();

        if constexpr (use_console)
        {
            FreeConsole();
        }

        FreeLibraryAndExitThread(module_handle, 0u);
        return 0;
    }
}


BOOL APIENTRY DllMain(const HMODULE module_handle, const DWORD reason, LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module_handle);

        const HANDLE thread{ CreateThread(
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(&worker_thread),
            module_handle,
            0,
            nullptr) };

        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
