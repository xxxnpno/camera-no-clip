#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>


namespace
{
    constexpr std::array<std::wstring_view, 3> target_process_names{
        L"javaw.exe",
        L"java.exe",
        L"javaw.exe",
    };

    auto find_target_pid() noexcept
        -> DWORD
    {
        const HANDLE snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            std::println("[ERROR] CreateToolhelp32Snapshot failed: {}", GetLastError());
            return 0;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        DWORD found_pid{ 0 };

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                for (const std::wstring_view candidate : target_process_names)
                {
                    if (_wcsicmp(entry.szExeFile, candidate.data()) == 0)
                    {
                        found_pid = entry.th32ProcessID;
                        break;
                    }
                }
                if (found_pid != 0)
                {
                    break;
                }
            }
            while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found_pid;
    }

    auto inject(const DWORD pid, const std::string& dll_path) noexcept
        -> bool
    {
        const HANDLE process{ OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE,
            pid) };

        if (process == nullptr)
        {
            std::println("[ERROR] OpenProcess({}) failed: {}", pid, GetLastError());
            return false;
        }

        const SIZE_T path_size{ dll_path.size() + 1 };

        LPVOID remote_buffer{ VirtualAllocEx(process, nullptr, path_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) };
        if (remote_buffer == nullptr)
        {
            std::println("[ERROR] VirtualAllocEx failed: {}", GetLastError());
            CloseHandle(process);
            return false;
        }

        if (!WriteProcessMemory(process, remote_buffer, dll_path.c_str(), path_size, nullptr))
        {
            std::println("[ERROR] WriteProcessMemory failed: {}", GetLastError());
            VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
            CloseHandle(process);
            return false;
        }

        // kernel32 is loaded at the same address in every Win32 process for
        // the same architecture, so LoadLibraryA's address in our process is
        // also valid in the target.
        const HMODULE kernel32{ GetModuleHandleA("kernel32.dll") };
        const FARPROC load_library{ kernel32 ? GetProcAddress(kernel32, "LoadLibraryA") : nullptr };
        if (load_library == nullptr)
        {
            std::println("[ERROR] GetProcAddress(LoadLibraryA) failed: {}", GetLastError());
            VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
            CloseHandle(process);
            return false;
        }

        const HANDLE remote_thread{ CreateRemoteThread(
            process,
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(load_library),
            remote_buffer,
            0,
            nullptr) };

        if (remote_thread == nullptr)
        {
            std::println("[ERROR] CreateRemoteThread failed: {}", GetLastError());
            VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
            CloseHandle(process);
            return false;
        }

        WaitForSingleObject(remote_thread, INFINITE);

        DWORD exit_code{ 0 };
        GetExitCodeThread(remote_thread, &exit_code);

        CloseHandle(remote_thread);
        VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
        CloseHandle(process);

        if (exit_code == 0)
        {
            std::println("[ERROR] Remote LoadLibraryA returned NULL - DLL load failed in target process");
            return false;
        }

        std::println("[INFO] Injected at remote HMODULE 0x{:X}", exit_code);
        return true;
    }
}


auto main(const int argc, const char** const argv)
    -> int
{
    namespace fs = std::filesystem;

    fs::path dll_path{ argc >= 2 ? argv[1] : "camera-no-clip.dll" };
    std::error_code ec{};
    dll_path = fs::absolute(dll_path, ec);
    if (ec)
    {
        std::println("[ERROR] absolute({}) failed: {}", dll_path.string(), ec.message());
        return 1;
    }
    if (!fs::exists(dll_path))
    {
        std::println("[ERROR] DLL not found at {}", dll_path.string());
        std::println("        usage: injector [path\\to\\camera-no-clip.dll]");
        return 1;
    }

    std::println("[INFO] DLL: {}", dll_path.string());
    std::println("[INFO] Searching for javaw.exe / java.exe...");

    const DWORD pid{ find_target_pid() };
    if (pid == 0)
    {
        std::println("[ERROR] No javaw.exe / java.exe process found.  Start Minecraft first.");
        return 1;
    }

    std::println("[INFO] Target PID: {}", pid);

    if (!inject(pid, dll_path.string()))
    {
        return 1;
    }

    std::println("[INFO] Done.");
    return 0;
}
