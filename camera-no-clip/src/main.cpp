#include <vmhook/vmhook.hpp>

#include <windows.h>

#include <atomic>
#include <chrono>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <thread>


namespace sdk
{
    // Type tags for vmhook::register_class<T>().  These classes deliberately
    // expose no methods: the hook only inspects the calling frame and writes
    // null into the return slot, so no field / method lookup is needed on the
    // wrapped Java objects.

    class world final : public vmhook::object<world>
    {
    public:
        explicit world(const vmhook::oop_type_t instance) noexcept
            : vmhook::object<world>{ instance }
        {
        }
    };

    class vec3 final : public vmhook::object<vec3>
    {
    public:
        explicit vec3(const vmhook::oop_type_t instance) noexcept
            : vmhook::object<vec3>{ instance }
        {
        }
    };

    class moving_object_position final : public vmhook::object<moving_object_position>
    {
    public:
        explicit moving_object_position(const vmhook::oop_type_t instance) noexcept
            : vmhook::object<moving_object_position>{ instance }
        {
        }
    };

    class entity_renderer final : public vmhook::object<entity_renderer>
    {
    public:
        explicit entity_renderer(const vmhook::oop_type_t instance) noexcept
            : vmhook::object<entity_renderer>{ instance }
        {
        }
    };
}


namespace mapping
{
    // The four name sets we need to install the hook and identify the caller.
    // We support both MCP (deobfuscated) and 1.8 OBF builds; SRG is omitted
    // because Hypixel-flavoured clients ship either MCP or OBF in practice.
    struct names
    {
        const char* world_class;
        const char* vec3_class;
        const char* mop_class;
        const char* entity_renderer_class;

        const char* world_signature;
        const char* vec3_signature;
        const char* mop_signature;

        const char* ray_trace_blocks;
        const char* orient_camera;
    };

    inline constexpr names mcp{
        "net/minecraft/world/World",
        "net/minecraft/util/Vec3",
        "net/minecraft/util/MovingObjectPosition",
        "net/minecraft/client/renderer/EntityRenderer",
        "Lnet/minecraft/world/World;",
        "Lnet/minecraft/util/Vec3;",
        "Lnet/minecraft/util/MovingObjectPosition;",
        "rayTraceBlocks",
        "orientCamera",
    };

    inline constexpr names obf{
        "adm",
        "aui",
        "auh",
        "bfk",
        "Ladm;",
        "Laui;",
        "Lauh;",
        "a",
        "f",
    };

    inline const names* current{ nullptr };

    static auto detect() noexcept
        -> bool
    {
        if (vmhook::find_class(mcp.world_class) != nullptr)
        {
            current = &mcp;
            std::println("[INFO] camera-no-clip: MCP mapping detected");
            return true;
        }
        if (vmhook::find_class(obf.world_class) != nullptr)
        {
            current = &obf;
            std::println("[INFO] camera-no-clip: OBF mapping detected");
            return true;
        }
        std::println("[ERROR] camera-no-clip: no recognised World class found - injected into wrong process?");
        return false;
    }
}


namespace
{
    std::atomic<bool> g_enabled{ true };
    std::atomic<bool> g_running{ true };

    // NOTE: deliberately NOT noexcept.  vmhook's function_traits only
    // specialises on non-noexcept free-function pointer types, so adding
    // `noexcept` to a free function passed to vmhook::hook<>() makes the
    // template fail to deduce args_tuple_t.  (Member function detours can
    // use noexcept because the member-fn-pointer specialisation handles it.)
    auto on_ray_trace_blocks(vmhook::return_value& return_value,
                             const std::unique_ptr<sdk::world>& /*world_instance*/,
                             const std::unique_ptr<sdk::vec3>& /*start*/,
                             const std::unique_ptr<sdk::vec3>& /*end*/,
                             bool /*stop_on_liquid*/,
                             bool /*ignore_no_bounding_box*/,
                             bool /*return_last_uncollidable*/)
        -> void
    {
        if (!g_enabled.load(std::memory_order_relaxed))
        {
            return;
        }

        const vmhook::return_value::caller_info caller{ return_value.caller() };
        if (!caller.valid())
        {
            return;
        }

        // Only short-circuit the ray trace when called from
        // EntityRenderer.orientCamera, the f3+f5 third-person camera
        // collision check.  Every other caller (block-break hit-test,
        // player reach calc, etc.) sees the real result.
        if (caller.class_name != mapping::current->entity_renderer_class)
        {
            return;
        }
        if (caller.method_name != mapping::current->orient_camera)
        {
            return;
        }

        return_value.set<sdk::moving_object_position>(nullptr);
    }
}


namespace camera_no_clip
{
    auto run() noexcept
        -> void
    {
        std::println("[INFO] camera-no-clip starting");

        if (!mapping::detect())
        {
            return;
        }

        vmhook::register_class<sdk::world>(mapping::current->world_class);
        vmhook::register_class<sdk::vec3>(mapping::current->vec3_class);
        vmhook::register_class<sdk::moving_object_position>(mapping::current->mop_class);
        vmhook::register_class<sdk::entity_renderer>(mapping::current->entity_renderer_class);

        const std::string ray_trace_signature{ std::format(
            "({}{}ZZZ){}",
            mapping::current->vec3_signature,
            mapping::current->vec3_signature,
            mapping::current->mop_signature) };

        const bool installed{ vmhook::hook<sdk::world>(
            mapping::current->ray_trace_blocks,
            ray_trace_signature,
            &on_ray_trace_blocks) };

        if (!installed)
        {
            std::println("[ERROR] camera-no-clip: hook install failed for {}.{}",
                         mapping::current->world_class,
                         mapping::current->ray_trace_blocks);
            return;
        }

        std::println("[INFO] camera-no-clip: hook installed - INSERT toggles, END unloads");

        bool prev_insert{ false };
        bool prev_end{ false };

        while (g_running.load(std::memory_order_relaxed))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

            const bool insert_down{ (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0 };
            if (insert_down && !prev_insert)
            {
                const bool new_state{ !g_enabled.load(std::memory_order_relaxed) };
                g_enabled.store(new_state, std::memory_order_relaxed);
                std::println("[INFO] camera-no-clip: {}", new_state ? "ENABLED" : "DISABLED");
            }
            prev_insert = insert_down;

            const bool end_down{ (GetAsyncKeyState(VK_END) & 0x8000) != 0 };
            if (end_down && !prev_end)
            {
                std::println("[INFO] camera-no-clip: unloading on user request");
                g_running.store(false, std::memory_order_relaxed);
            }
            prev_end = end_down;
        }

        vmhook::shutdown_hooks();
        std::println("[INFO] camera-no-clip stopped");
    }
}
