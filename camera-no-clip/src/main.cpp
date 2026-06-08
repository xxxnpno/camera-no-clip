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
    // The name sets we need to install the hook, identify the caller frame,
    // and disambiguate MCP from SRG at startup.
    //
    // Three mappings are supported:
    //   - MCP : deobfuscated names (vanilla 1.8.9, most Hypixel clients,
    //           Lunar, Badlion).
    //   - SRG : Searge names used by Forge at runtime - classes keep the MCP
    //           form but every method / field is renamed to func_*****_x /
    //           field_*****_x.  MCP and SRG cannot be told apart by class
    //           lookup alone, so detect() probes World.playerEntities (MCP)
    //           vs World.field_73010_i (SRG) after registering the class.
    //   - OBF : 1.8.9 obfuscated names (rare in modern launchers, but still
    //           present on bare vanilla jars).
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

        // Field on World used only by detect() to tell MCP from SRG (both
        // share the same World class name).  Not used by the hook itself.
        const char* probe_field;
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
        "playerEntities",
    };

    inline constexpr names srg{
        "net/minecraft/world/World",
        "net/minecraft/util/Vec3",
        "net/minecraft/util/MovingObjectPosition",
        "net/minecraft/client/renderer/EntityRenderer",
        "Lnet/minecraft/world/World;",
        "Lnet/minecraft/util/Vec3;",
        "Lnet/minecraft/util/MovingObjectPosition;",
        "func_147447_a",
        "func_78467_g",
        "field_73010_i",
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
        "j",
    };

    inline const names* current{ nullptr };

    static auto detect() noexcept
        -> bool
    {
        // Step 1: MCP and SRG share class names.  If the MCP-named World
        // class is present, we register it once and then probe one field on
        // it - playerEntities (MCP) vs field_73010_i (SRG) - to know which
        // mapping is in effect.
        //
        // The probe goes through vmhook::find_field directly (not the
        // higher-level sdk::world::get_field), because the latter is
        // static-only and World.playerEntities is an instance field.
        if (vmhook::hotspot::klass* const world_klass{ vmhook::find_class(mcp.world_class) })
        {
            vmhook::register_class<sdk::world>(mcp.world_class);

            if (vmhook::find_field(world_klass, mcp.probe_field).has_value())
            {
                current = &mcp;
                std::println("[INFO] camera-no-clip: MCP mapping detected");
                return true;
            }
            if (vmhook::find_field(world_klass, srg.probe_field).has_value())
            {
                current = &srg;
                std::println("[INFO] camera-no-clip: SRG (Forge) mapping detected");
                return true;
            }
            std::println("[WARN] camera-no-clip: World class found but neither "
                         "MCP nor SRG probe field present; falling back to MCP");
            current = &mcp;
            return true;
        }

        // Step 2: OBF 1.8 has differently-named classes.
        if (vmhook::find_class(obf.world_class) != nullptr)
        {
            vmhook::register_class<sdk::world>(obf.world_class);
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

    // Intentionally empty - see the install site in run().  This detour exists
    // purely for its side effect: hooking a method forces vmhook to run it in
    // the interpreter, and return_value::caller() can only walk the saved-rbp
    // chain when the *calling* frame is interpreted too.  Left un-hooked,
    // EntityRenderer.orientCamera stays JIT-compiled, so the caller() read
    // inside on_ray_trace_blocks lands on a compiled frame, comes back empty
    // (caller.valid() == false), and the detour bails before nulling the camera
    // ray - the no-clip then silently never fires.  Same non-noexcept
    // requirement as on_ray_trace_blocks (vmhook's free-function-pointer traits).
    auto on_orient_camera(vmhook::return_value& /*return_value*/,
                          const std::unique_ptr<sdk::entity_renderer>& /*renderer*/,
                          float /*partial_ticks*/)
        -> void
    {
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

        // sdk::world is already registered inside detect() (it had to be in
        // order to probe a field for MCP / SRG disambiguation).  The other
        // three types are pure caller-name / return-value tags and have not
        // been registered yet.
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

        // Carries no logic of its own (see on_orient_camera) - this hook only
        // forces EntityRenderer.orientCamera into the interpreter so that the
        // caller() check inside on_ray_trace_blocks can recognise it as the
        // caller.  Without it the no-clip never fires even though the
        // rayTraceBlocks hook installed fine.
        //
        // The (F)V signature is passed explicitly: in obfuscated (vanilla)
        // mappings orientCamera is named "f", and the obfuscator reuses that
        // name for several methods on EntityRenderer.  A name-only hook matches
        // the first "f" in the methods array (the wrong method), so the deopt
        // never reaches the real orientCamera and the no-clip stays dead on
        // vanilla while still working on MCP/SRG.  The descriptor is (F)V across
        // all three mappings (the lone parameter is a primitive float), so it
        // disambiguates everywhere without needing a per-mapping signature.
        if (!vmhook::hook<sdk::entity_renderer>(mapping::current->orient_camera, "(F)V", &on_orient_camera))
        {
            std::println("[WARN] camera-no-clip: failed to hook {}.{} - no-clip may stay inactive",
                         mapping::current->entity_renderer_class,
                         mapping::current->orient_camera);
        }

        std::println("[INFO] camera-no-clip: hook installed - DELETE toggles, END unloads");

        bool prev_delete{ false };
        bool prev_end{ false };

        while (g_running.load(std::memory_order_relaxed))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

            const bool delete_down{ (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0 };
            if (delete_down && !prev_delete)
            {
                const bool new_state{ !g_enabled.load(std::memory_order_relaxed) };
                g_enabled.store(new_state, std::memory_order_relaxed);
                std::println("[INFO] camera-no-clip: {}", new_state ? "ENABLED" : "DISABLED");
            }
            prev_delete = delete_down;

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
