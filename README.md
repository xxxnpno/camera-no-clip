# camera-no-clip

Minimal Minecraft 1.8.9 mod that removes third-person camera collision (no-clip
camera) by hooking `World.rayTraceBlocks` via [vmhook](https://github.com/xxxnpno/vmhook).
No JNI, no JVMTI, no Forge/Fabric – injected as a plain Windows DLL.

When `EntityRenderer.orientCamera` calls `World.rayTraceBlocks` to figure out
where to put the third-person camera, the hook returns `null` (no collision)
so the camera passes through walls. Every other caller of `rayTraceBlocks`
(block-break reach, entity hit-test, player interactions) is left alone.

## How it works

`main.cpp` installs exactly one vmhook hook:

```cpp
vmhook::hook<sdk::world>("rayTraceBlocks", signature, &on_ray_trace_blocks);
```

The detour inspects the calling frame via `vmhook::return_value::caller()`. If
the caller is `EntityRenderer.orientCamera`, it writes `null` into the return
slot and lets the JVM continue. Anything else falls through unchanged.

Three name schemes are supported (auto-detected at startup):

- **MCP** — vanilla 1.8.9, most Hypixel clients, Lunar, Badlion.
- **SRG** — Searge names used by Forge at runtime (`func_*` / `field_*`).
- **OBF** — 1.8.9 obfuscated jars (e.g. `adm`, `bfk`).

MCP and SRG share class names, so detection probes `World.playerEntities`
(MCP) vs `World.field_73010_i` (SRG) to tell them apart.

vmhook v0.5.0 ships an auto-repair watchdog, so the hook keeps firing even
after HotSpot's tiered compiler eventually re-JITs `rayTraceBlocks`.

## Build

Requires Visual Studio 2026 (or any toolchain with `v145` / C++23).

```
msbuild camera-no-clip.slnx -p:Configuration=Release -p:Platform=x64
```

Outputs:

- `build/camera-no-clip.dll`   – the mod
- `build/camera-no-clip-injector.exe` – tiny console injector

## Usage

1. Launch Minecraft (1.8.9 client) and wait until the main menu is showing.
2. Run `build\camera-no-clip-injector.exe`. A small ImGui window lists every
   running `javaw.exe` / `java.exe` by window title and PID; click one to
   inject. The DLL is embedded inside the injector as an RCDATA resource,
   so the .exe is the only file you need to ship. (If a sibling
   `camera-no-clip.dll` is found next to the .exe, that one is preferred
   over the embedded copy — useful while iterating on the DLL build.)
3. A small console window pops up inside Minecraft's process with the
   mod's log output.

4. In-game hotkeys:
   - **INSERT** – toggle camera no-clip on / off
   - **END**    – cleanly unload (uninstall hook, free the DLL)

## Layout

```
camera-no-clip/
├── camera-no-clip/
│   ├── ext/vmhook/vmhook.hpp   single-header vmhook v0.5.0 (vendored)
│   └── src/
│       ├── main.cpp             SDK type tags, mapping, hook detour, hotkey loop
│       └── dllmain.cpp          DLL entry; spawns worker thread, attaches console
└── injector/
    ├── ext/imgui/               imgui + DX11/Win32 backends (vendored)
    ├── ext/idr.rc               embeds camera-no-clip.dll as RCDATA resource 256
    └── src/main/
        ├── main.cpp             Win32 window, message loop
        ├── dx11.cpp/.hpp        D3D11 swap chain init / cleanup
        ├── injector.cpp/.hpp    Enumerate javaw windows, LoadLibrary via CreateRemoteThread
        └── ui.cpp/.hpp          ImGui target-picker UI
```

## License

MIT. vmhook is also MIT (see [vmhook.hpp](camera-no-clip/ext/vmhook/vmhook.hpp)
header).
