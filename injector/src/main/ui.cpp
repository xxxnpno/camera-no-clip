#include "ui.hpp"

#include <cmath>

#include <imgui/imgui.h>

namespace
{
    constexpr ImU32 panel_color{ IM_COL32(23, 23, 26, 255) };
    constexpr ImU32 panel_border_color{ IM_COL32(36, 36, 39, 255) };
    constexpr ImU32 card_color{ IM_COL32(32, 32, 36, 255) };
    constexpr ImU32 card_hovered_color{ IM_COL32(39, 39, 44, 255) };
    constexpr ImU32 card_active_color{ IM_COL32(26, 38, 36, 255) };
    constexpr ImU32 card_border_color{ IM_COL32(66, 66, 74, 255) };
    constexpr ImU32 accent_color{ IM_COL32(95, 235, 255, 255) };

    auto with_alpha(const ImU32 color, const float alpha) noexcept
        -> ImU32
    {
        const int a{ static_cast<int>(((color >> IM_COL32_A_SHIFT) & 0xFF) * alpha) };
        return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }

    auto ease_out_cubic(const float t) noexcept
        -> float
    {
        const float inv{ 1.f - t };
        return 1.f - inv * inv * inv;
    }

    auto ease_in_out_cubic(const float t) noexcept
        -> float
    {
        return t < 0.5f
            ? 4.f * t * t * t
            : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
    }

    auto ease_out_back(const float t) noexcept
        -> float
    {
        constexpr float c1{ 1.70158f };
        constexpr float c3{ c1 + 1.f };
        return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
    }

    auto text_centered(const char* text, float y) noexcept
        -> void
    {
        const ImVec2 size{ ImGui::CalcTextSize(text) };
        const float x{ (ImGui::GetIO().DisplaySize.x - size.x) * 0.5f };
        ImGui::SetCursorPos({ x, y });
        ImGui::TextUnformatted(text);
    }

    auto draw_background(const ImVec2 display, const float alpha, const float scale_x, const float scale_y, const float y_offset) noexcept
        -> void
    {
        ImDrawList* draw{ ImGui::GetWindowDrawList() };
        const ImVec2 pos{ ImGui::GetWindowPos() };
        const ImVec2 center{ pos.x + display.x * 0.5f, pos.y + display.y * 0.5f + y_offset };
        const ImVec2 half{ (display.x - 36.f) * 0.5f * scale_x, (display.y - 32.f) * 0.5f * scale_y };
        const ImVec2 min{ center.x - half.x, center.y - half.y };
        const ImVec2 max{ center.x + half.x, center.y + half.y };

        draw->AddRectFilled(min, max, with_alpha(panel_color, alpha), 7.f);
        draw->AddRect(min, max, with_alpha(panel_border_color, alpha), 7.f, 0, 2.f);
        draw->AddRect(
            { min.x + 2.f, min.y + 2.f },
            { max.x - 2.f, max.y - 2.f },
            with_alpha(IM_COL32(28, 28, 31, 255), alpha),
            5.f
        );
    }

    auto draw_success_fx(const ui::state& app_state, const ImVec2 display, const float progress) noexcept
        -> void
    {
        ImDrawList* draw{ ImGui::GetWindowDrawList() };
        const ImVec2 pos{ ImGui::GetWindowPos() };
        const ImVec2 origin{ app_state.close_origin_x, app_state.close_origin_y };

        const float pulse_t{ progress < 0.42f ? progress / 0.42f : 1.f };
        const float pulse{ ease_out_cubic(pulse_t) };
        const float pulse_alpha{ (1.f - pulse_t) * 0.72f };
        draw->AddCircle(origin, 30.f + 380.f * pulse, with_alpha(accent_color, pulse_alpha), 96, 3.f);

        const float line_t_raw{ progress < 0.34f ? 0.f : (progress - 0.34f) / 0.66f };
        const float line_t{ line_t_raw > 1.f ? 1.f : line_t_raw };
        const float line_eased{ ease_in_out_cubic(line_t) };
        const float line_width{ (display.x - 130.f) * (1.f - line_eased) };
        if (line_width > 6.f)
        {
            const float y{ pos.y + display.y * 0.5f - 1.f };
            draw->AddRectFilled(
                { pos.x + (display.x - line_width) * 0.5f, y },
                { pos.x + (display.x + line_width) * 0.5f, y + 3.f },
                with_alpha(accent_color, 0.85f * (1.f - line_eased)),
                2.f
            );
        }
    }

    auto draw_open_fx(const ImVec2 display, const float progress) noexcept
        -> void
    {
        if (progress >= 1.f)
        {
            return;
        }

        ImDrawList* draw{ ImGui::GetWindowDrawList() };
        const ImVec2 pos{ ImGui::GetWindowPos() };
        const float sweep{ ease_out_cubic(progress) };
        const float alpha{ (1.f - progress) * 0.70f };
        const float width{ (display.x - 130.f) * sweep };
        const float x{ pos.x + (display.x - width) * 0.5f };
        const float y{ pos.y + display.y * 0.5f - 1.f };

        draw->AddRectFilled(
            { x, y },
            { x + width, y + 3.f },
            with_alpha(accent_color, alpha),
            2.f
        );
    }

    auto fit_text(std::string text, const float max_width) noexcept
        -> std::string
    {
        if (ImGui::CalcTextSize(text.c_str()).x <= max_width)
        {
            return text;
        }

        constexpr char suffix[]{ "..." };
        while (!text.empty())
        {
            text.pop_back();
            const std::string candidate{ text + suffix };
            if (ImGui::CalcTextSize(candidate.c_str()).x <= max_width)
            {
                return candidate;
            }
        }

        return suffix;
    }

    auto inject_target(ui::state& app_state, const injector::javaw_window& window, const ImVec2 origin) noexcept
        -> void
    {
        const std::string dll_path{ injector::extract_dll_to_temp() };
        if (dll_path.empty())
        {
            app_state.status = "Failed to extract DLL";
            app_state.status_ok = false;
        }
        else if (injector::inject_dll(window.process_id, dll_path.c_str()))
        {
            app_state.status.clear();
            app_state.status_ok = true;
            app_state.closing = true;
            app_state.close_start_time = ImGui::GetTime();
            app_state.close_origin_x = origin.x;
            app_state.close_origin_y = origin.y;
        }
        else
        {
            app_state.status = "Injection failed";
            app_state.status_ok = false;
        }
    }

    auto render_target_card(ui::state& app_state, const injector::javaw_window& window, const std::int32_t index, const float alpha) noexcept
        -> void
    {
        constexpr ImVec2 size{ 190.f, 48.f };
        const std::string id{ "##target_" + std::to_string(index) };

        ImGui::InvisibleButton(id.c_str(), size);

        const bool hovered{ ImGui::IsItemHovered() };
        const bool active{ ImGui::IsItemActive() };
        if (!app_state.closing && ImGui::IsItemClicked())
        {
            const ImVec2 item_min{ ImGui::GetItemRectMin() };
            const ImVec2 item_max{ ImGui::GetItemRectMax() };
            inject_target(app_state, window, { (item_min.x + item_max.x) * 0.5f, (item_min.y + item_max.y) * 0.5f });
        }

        const ImVec2 min{ ImGui::GetItemRectMin() };
        const ImVec2 max{ ImGui::GetItemRectMax() };
        ImDrawList* draw{ ImGui::GetWindowDrawList() };
        draw->AddRectFilled(min, max, with_alpha(active ? card_active_color : (hovered ? card_hovered_color : card_color), alpha), 4.f);
        draw->AddRect(min, max, with_alpha(card_border_color, alpha), 4.f);

        const std::string title{ fit_text(window.title, size.x - 18.f) };
        const std::string pid{ "PID " + std::to_string(window.process_id) };
        const ImVec2 title_size{ ImGui::CalcTextSize(title.c_str()) };
        const ImVec2 pid_size{ ImGui::CalcTextSize(pid.c_str()) };

        draw->AddText(
            { min.x + (size.x - title_size.x) * 0.5f, min.y + 8.f },
            with_alpha(IM_COL32(242, 242, 245, 255), alpha),
            title.c_str()
        );
        draw->AddText(
            { min.x + (size.x - pid_size.x) * 0.5f, min.y + 26.f },
            with_alpha(IM_COL32(133, 133, 140, 255), alpha),
            pid.c_str()
        );
    }
}

namespace ui
{
    auto apply_style() noexcept
        -> void
    {
        ImGuiStyle& style{ ImGui::GetStyle() };

        style.WindowPadding = { 0.f, 0.f };
        style.FramePadding = { 13.f, 8.f };
        style.ItemSpacing = { 10.f, 8.f };
        style.ItemInnerSpacing = { 8.f, 6.f };
        style.WindowRounding = 0.f;
        style.ChildRounding = 5.f;
        style.FrameRounding = 4.f;
        style.PopupRounding = 4.f;
        style.ScrollbarRounding = 4.f;
        style.GrabRounding = 4.f;
        style.WindowBorderSize = 0.f;
        style.FrameBorderSize = 0.f;

        ImVec4* colors{ style.Colors };
        colors[ImGuiCol_Text] = { 0.94f, 0.94f, 0.96f, 1.f };
        colors[ImGuiCol_TextDisabled] = { 0.45f, 0.45f, 0.49f, 1.f };
        colors[ImGuiCol_WindowBg] = { 0.07f, 0.07f, 0.08f, 1.f };
        colors[ImGuiCol_PopupBg] = { 0.10f, 0.10f, 0.12f, 1.f };
        colors[ImGuiCol_FrameBg] = { 0.08f, 0.08f, 0.09f, 1.f };
        colors[ImGuiCol_FrameBgHovered] = { 0.12f, 0.12f, 0.14f, 1.f };
        colors[ImGuiCol_FrameBgActive] = { 0.14f, 0.14f, 0.16f, 1.f };
        colors[ImGuiCol_Button] = { 0.00f, 0.57f, 0.45f, 1.f };
        colors[ImGuiCol_ButtonHovered] = { 0.00f, 0.70f, 0.55f, 1.f };
        colors[ImGuiCol_ButtonActive] = { 0.00f, 0.48f, 0.38f, 1.f };
        colors[ImGuiCol_Header] = { 0.00f, 0.57f, 0.45f, 0.28f };
        colors[ImGuiCol_HeaderHovered] = { 0.00f, 0.57f, 0.45f, 0.42f };
        colors[ImGuiCol_HeaderActive] = { 0.00f, 0.57f, 0.45f, 0.60f };
        colors[ImGuiCol_Border] = { 0.18f, 0.18f, 0.20f, 1.f };
        colors[ImGuiCol_Separator] = { 0.16f, 0.16f, 0.18f, 1.f };
    }

    auto render(HWND hwnd, state& app_state) noexcept
        -> void
    {
        const ImVec2 display{ ImGui::GetIO().DisplaySize };
        const double now{ ImGui::GetTime() };
        if (app_state.open_start_time == 0.0)
        {
            app_state.open_start_time = now;
        }

        if (!app_state.closing && now - app_state.last_refresh_time >= 1.0)
        {
            app_state.windows = injector::refresh_windows();
            app_state.last_refresh_time = now;
        }

        const float open_t_raw{ static_cast<float>((now - app_state.open_start_time) / 0.62) };
        const float open_t{ open_t_raw > 1.f ? 1.f : open_t_raw };
        const float open_eased{ ease_out_back(open_t) };
        const float content_t_raw{ open_t < 0.38f ? 0.f : (open_t - 0.38f) / 0.62f };
        const float content_alpha{ ease_out_cubic(content_t_raw > 1.f ? 1.f : content_t_raw) };

        float alpha{ open_t };
        float scale_x{ 0.82f + 0.18f * open_eased };
        float scale_y{ 0.025f + 0.975f * open_eased };
        float y_offset{ 0.f };
        float progress{ 0.f };
        if (app_state.closing)
        {
            const float t{ static_cast<float>((now - app_state.close_start_time) / 0.82) };
            progress = t > 1.f ? 1.f : t;

            const float collapse_raw{ progress < 0.22f ? 0.f : (progress - 0.22f) / 0.78f };
            const float collapse{ ease_in_out_cubic(collapse_raw > 1.f ? 1.f : collapse_raw) };
            const float fade_raw{ progress < 0.12f ? 0.f : (progress - 0.12f) / 0.88f };
            const float fade{ ease_out_cubic(fade_raw > 1.f ? 1.f : fade_raw) };

            alpha = 1.f - fade;
            scale_x = 1.f - 0.82f * collapse;
            scale_y = 1.f - 0.985f * collapse;
            y_offset = -10.f * collapse;
            if (t >= 1.f)
            {
                PostQuitMessage(0);
            }
        }

        ImGui::SetNextWindowPos({ 0.f, 0.f });
        ImGui::SetNextWindowSize(display);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoBackground
        );

        draw_background(display, alpha, scale_x, scale_y, y_offset);
        if (!app_state.closing)
        {
            draw_open_fx(display, open_t);
        }
        if (app_state.closing)
        {
            draw_success_fx(app_state, display, progress);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha * content_alpha);
        text_centered("Select a Minecraft to use", 52.f);
        text_centered("Make sure game is fully loaded first", 68.f);

        const float card_x{ (display.x - 190.f) * 0.5f };
        float y{ 104.f };
        if (app_state.windows.empty())
        {
            text_centered("Waiting for javaw.exe...", y + 12.f);
        }
        else
        {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(app_state.windows.size()); ++i)
            {
                ImGui::SetCursorPos({ card_x, y });
                render_target_card(app_state, app_state.windows[i], i, alpha);
                y += 54.f;
            }
        }

        if (!app_state.status.empty())
        {
            const ImVec4 color{ app_state.status_ok ? ImVec4{ 0.20f, 0.86f, 0.56f, 1.f } : ImVec4{ 0.92f, 0.30f, 0.30f, 1.f } };
            const float width{ ImGui::CalcTextSize(app_state.status.c_str()).x };
            ImGui::SetCursorPosX((display.x - width) * 0.5f);
            ImGui::SetCursorPosY(display.y - 46.f);
            ImGui::TextColored(color, "%s", app_state.status.c_str());
        }
        ImGui::PopStyleVar();

        ImGui::End();
    }
}
