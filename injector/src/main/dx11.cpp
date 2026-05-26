#include "dx11.hpp"

namespace dx11
{
    auto create_render_target() noexcept
        -> void
    {
        ID3D11Texture2D* back_buffer{ nullptr };
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        device->CreateRenderTargetView(back_buffer, nullptr, &render_target);
        back_buffer->Release();
    }

    auto cleanup_render_target() noexcept
        -> void
    {
        if (render_target)
        {
            render_target->Release();
            render_target = nullptr;
        }
    }

    auto init(HWND hwnd) noexcept
        -> bool
    {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL feature_levels[]{ D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL feature_level{};

        if (D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            feature_levels,
            1,
            D3D11_SDK_VERSION,
            &sd,
            &swap_chain,
            &device,
            &feature_level,
            &device_context
        ) != S_OK)
        {
            return false;
        }

        create_render_target();
        return true;
    }

    auto cleanup() noexcept
        -> void
    {
        cleanup_render_target();
        if (swap_chain)
        {
            swap_chain->Release();
            swap_chain = nullptr;
        }

        if (device_context)
        {
            device_context->Release();
            device_context = nullptr;
        }

        if (device)
        {
            device->Release();
            device = nullptr;
        }
    }
}
