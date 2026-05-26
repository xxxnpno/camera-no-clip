#include "injector.hpp"

#include <filesystem>
#include <tlhelp32.h>

namespace
{
    auto is_javaw_process(const DWORD process_id) noexcept
        -> bool
    {
        const HANDLE snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        bool found{ false };
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (entry.th32ProcessID == process_id)
                {
                    found = (std::wstring{ entry.szExeFile } == L"javaw.exe") || (std::wstring{ entry.szExeFile } == L"java.exe");
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found;
    }

    BOOL CALLBACK enum_windows_callback(HWND hwnd, LPARAM lParam)
    {
        if (!IsWindowVisible(hwnd))
        {
            return TRUE;
        }

        char title[512]{};
        if (!GetWindowTextA(hwnd, title, sizeof(title)) || title[0] == '\0')
        {
            return TRUE;
        }

        DWORD process_id{ 0 };
        GetWindowThreadProcessId(hwnd, &process_id);

        if (is_javaw_process(process_id))
        {
            auto* windows{ reinterpret_cast<std::vector<injector::javaw_window>*>(lParam) };
            windows->push_back({ process_id, title });
        }

        return TRUE;
    }
}

namespace injector
{
    auto extract_dll_to_temp() noexcept
        -> std::string
    {
        char executable_path[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, executable_path, MAX_PATH))
        {
            const std::filesystem::path debug_dll{
                std::filesystem::path{ executable_path }.parent_path() / "camera-no-clip.dll"
            };

            std::error_code error{};
            if (std::filesystem::exists(debug_dll, error))
            {
                return debug_dll.string();
            }
        }

        const HRSRC resource{ FindResourceW(nullptr, MAKEINTRESOURCEW(256), MAKEINTRESOURCEW(10)) };
        if (!resource)
        {
            return {};
        }

        const HGLOBAL resource_data{ LoadResource(nullptr, resource) };
        if (!resource_data)
        {
            return {};
        }

        const void* data{ LockResource(resource_data) };
        const DWORD size{ SizeofResource(nullptr, resource) };

        char temp_path[MAX_PATH]{};
        char temp_file[MAX_PATH]{};
        GetTempPathA(MAX_PATH, temp_path);
        GetTempFileNameA(temp_path, "dll", 0, temp_file);

        std::filesystem::path temp_dll{ temp_file };
        temp_dll.replace_extension(".dll");
        DeleteFileA(temp_file);

        const HANDLE file{ CreateFileA(temp_dll.string().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
        if (file == INVALID_HANDLE_VALUE)
        {
            return {};
        }

        DWORD written{ 0 };
        WriteFile(file, data, size, &written, nullptr);
        CloseHandle(file);

        return temp_dll.string();
    }

    auto inject_dll(const DWORD process_id, const char* dll_path) noexcept
        -> bool
    {
        const HANDLE process{ OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id) };
        if (!process || process == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const std::size_t path_len{ strlen(dll_path) + 1 };
        void* remote_memory{ VirtualAllocEx(process, nullptr, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) };
        if (!remote_memory)
        {
            CloseHandle(process);
            return false;
        }

        if (!WriteProcessMemory(process, remote_memory, dll_path, path_len, nullptr))
        {
            VirtualFreeEx(process, remote_memory, 0, MEM_RELEASE);
            CloseHandle(process);
            return false;
        }

        const HANDLE thread{ CreateRemoteThread(
            process,
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA),
            remote_memory,
            0,
            nullptr
        ) };

        if (!thread)
        {
            VirtualFreeEx(process, remote_memory, 0, MEM_RELEASE);
            CloseHandle(process);
            return false;
        }

        WaitForSingleObject(thread, INFINITE);

        DWORD exit_code{ 0 };
        GetExitCodeThread(thread, &exit_code);

        CloseHandle(thread);
        VirtualFreeEx(process, remote_memory, 0, MEM_RELEASE);
        CloseHandle(process);

        return exit_code != 0;
    }

    auto refresh_windows() noexcept
        -> std::vector<javaw_window>
    {
        std::vector<javaw_window> windows;
        EnumWindows(enum_windows_callback, reinterpret_cast<LPARAM>(&windows));
        return windows;
    }
}
