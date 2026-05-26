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

MCP and 1.8 OBF class names are both supported (auto-detected at startup);
SRG would need adding the SRG names to `mapping::names` in `main.cpp`.

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

1. Launch Minecraft (1.8.9 client).
2. Run the injector:
   ```
   build\camera-no-clip-injector.exe build\camera-no-clip.dll
   ```
   The injector finds the first `javaw.exe` / `java.exe` and loads the DLL
   into it via `CreateRemoteThread(LoadLibraryA)`. A small console window
   pops up inside Minecraft's process with the mod's log output.

3. In-game hotkeys:
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
    └── src/main.cpp             Find javaw.exe, LoadLibraryA via CreateRemoteThread
```

## License

MIT. vmhook is also MIT (see [vmhook.hpp](camera-no-clip/ext/vmhook/vmhook.hpp)
header).
