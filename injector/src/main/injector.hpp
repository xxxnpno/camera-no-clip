#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace injector
{
    struct javaw_window
    {
        DWORD process_id{};
        std::string title{};
    };

    auto extract_dll_to_temp() noexcept
        -> std::string;

    auto inject_dll(DWORD process_id, const char* dll_path) noexcept
        -> bool;

    auto refresh_windows() noexcept
        -> std::vector<javaw_window>;
}
