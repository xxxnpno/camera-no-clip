#pragma once

#include <d3d11.h>
#include <windows.h>

namespace dx11
{
    inline ID3D11Device* device{ nullptr };
    inline ID3D11DeviceContext* device_context{ nullptr };
    inline IDXGISwapChain* swap_chain{ nullptr };
    inline ID3D11RenderTargetView* render_target{ nullptr };

    auto create_render_target() noexcept
        -> void;

    auto cleanup_render_target() noexcept
        -> void;

    auto init(HWND hwnd) noexcept
        -> bool;

    auto cleanup() noexcept
        -> void;
}
