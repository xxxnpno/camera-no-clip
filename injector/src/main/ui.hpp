#pragma once

#include "injector.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace ui
{
    struct state
    {
        std::vector<injector::javaw_window> windows{};
        std::string status{};
        bool status_ok{ false };
        double last_refresh_time{ 0.0 };
        bool closing{ false };
        double close_start_time{ 0.0 };
        float close_origin_x{ 0.f };
        float close_origin_y{ 0.f };
        double open_start_time{ 0.0 };
    };

    auto apply_style() noexcept
        -> void;

    auto render(HWND hwnd, state& app_state) noexcept
        -> void;
}
