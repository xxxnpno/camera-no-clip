#include "dx11.hpp"
#include "injector.hpp"
#include "ui.hpp"

#include <windows.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    bool dragging{ false };
    POINT drag_cursor{};
    POINT drag_window{};

    auto apply_window_region(HWND hwnd) noexcept
        -> void
    {
        RECT rect{};
        GetClientRect(hwnd, &rect);

        HRGN region{ CreateRoundRectRgn(
            18,
            16,
            rect.right - 18,
            rect.bottom - 16,
            14,
            14
        ) };

        SetWindowRgn(hwnd, region, TRUE);
    }

    auto centered_window_position(const int width, const int height) noexcept
        -> POINT
    {
        RECT work_area{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);

        return {
            work_area.left + ((work_area.right - work_area.left) - width) / 2,
            work_area.top + ((work_area.bottom - work_area.top) - height) / 2
        };
    }

    LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        {
            return true;
        }

        switch (msg)
        {
        case WM_SIZE:
            apply_window_region(hwnd);
            if (dx11::device && wParam != SIZE_MINIMIZED)
            {
                dx11::cleanup_render_target();
                dx11::swap_chain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                dx11::create_render_target();
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (HIWORD(lParam) < 74)
            {
                dragging = true;
                GetCursorPos(&drag_cursor);

                RECT rect{};
                GetWindowRect(hwnd, &rect);
                drag_window = { rect.left, rect.top };
                SetCapture(hwnd);
            }
            break;
        case WM_MOUSEMOVE:
            if (dragging)
            {
                POINT cursor{};
                GetCursorPos(&cursor);
                SetWindowPos(
                    hwnd,
                    nullptr,
                    drag_window.x + (cursor.x - drag_cursor.x),
                    drag_window.y + (cursor.y - drag_cursor.y),
                    0,
                    0,
                    SWP_NOSIZE | SWP_NOZORDER
                );
            }
            break;
        case WM_LBUTTONUP:
            if (dragging)
            {
                dragging = false;
                ReleaseCapture();
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

INT WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, INT)
{
    constexpr int window_width{ 858 };
    constexpr int window_height{ 510 };
    const POINT window_pos{ centered_window_position(window_width, window_height) };

    WNDCLASSEXW wc{};
    const auto icon{ reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(101),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR
    )) };
    const auto small_icon{ reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(101),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    )) };
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hIcon = icon;
    wc.hIconSm = small_icon;
    wc.lpszClassName = L"camera-no-clip";
    RegisterClassExW(&wc);

    const HWND hwnd{ CreateWindowExW(
        0,
        wc.lpszClassName,
        L"camera-no-clip",
        WS_POPUP,
        window_pos.x,
        window_pos.y,
        window_width,
        window_height,
        nullptr,
        nullptr,
        instance,
        nullptr
    ) };

    apply_window_region(hwnd);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));

    if (!dx11::init(hwnd))
    {
        dx11::cleanup();
        UnregisterClassW(wc.lpszClassName, instance);
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ui::apply_style();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dx11::device, dx11::device_context);

    ui::state app_state{};
    app_state.windows = injector::refresh_windows();

    const ImVec4 clear_color{ 0.07f, 0.07f, 0.08f, 1.00f };

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ui::render(hwnd, app_state);

        ImGui::Render();

        const float clear[]{ clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        dx11::device_context->OMSetRenderTargets(1, &dx11::render_target, nullptr);
        dx11::device_context->ClearRenderTargetView(dx11::render_target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        dx11::swap_chain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    dx11::cleanup();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, instance);

    return EXIT_SUCCESS;
}
