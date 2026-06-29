# AGENTS.md — CandyEngine

**语言：AI 回答请使用中文**

## Build System

- **Build tool:** premake5 (not CMake, not MSBuild). Config: `premake5.lua` at root.
- **Language:** C++17, **Windows x64 only** (`architecture "x64"`).
- **Config modes:** `Debug` → `CANDY_DEBUG`, `Release` → `CANDY_RELEASE`, `Dist` → `CANDY_DIST`.

```pwsh
# Generate VS solution (first step — .sln is gitignored)
.\Scripts\GenerateProjects.bat

# Then open CandyEngine.sln and build in VS, or build from command line:
msbuild CandyEngine.sln /p:Configuration=Debug

# Run (output goes to bin/<config>-windows-x86_64/<Project>/)
.\bin\Debug-windows-x86_64\Candy_Editor\Candy_Editor.exe
```

**Important:** `CandyEngine.sln` and all `.vcxproj` files are gitignored. Run `Scripts\GenerateProjects.bat` to regenerate them via premake. Never edit generated project files directly.

**Sandbox is commented out** in root `premake5.lua:38`. Uncomment to build it.

## Third-Party Dependencies

All third-party libs live in `Candy/ThirdParty/`:

| Dependency | Type | How it builds |
|---|---|---|
| **GLFW** | git submodule (forked: `xltangcs/glfw`) | Separate premake project → `GLFW` static lib |
| **glm** | git submodule (`g-truc/glm`) | Compiled as part of `Candy` (header + inline) |
| **imgui** | git submodule (forked: `xltangcs/imgui`) | Separate premake project → `Imgui` static lib |
| **ImGuizmo** | git submodule (forked: `xltangcs/ImGuizmo`) | Source compiled in `Candy` via `NoPCH` filter |
| **yaml-cpp** | git submodule (forked: `xltangcs/yaml-cpp`) | Separate premake project → `yaml-cpp` static lib |
| **box2d** | git submodule (forked: `xltangcs/box2d`) | Separate premake project → `box2d` static lib |
| **GLAD** | vendored source | Separate premake project → `GLAD` static lib (C source) |
| **stb_image** | vendored source | Compiled as part of `Candy` |
| **entt** | git submodule (header-only) | Include path only, no compilation |
| **spdlog** | vendored (header-only, `Candy/ThirdParty/spdlog/include`) | Include path only |

After first clone:
```pwsh
git submodule update --init --recursive
.\Scripts\GenerateProjects.bat
```

## Project Structure

| Directory | Target | Kind | Purpose |
|---|---|---|---|
| `Candy/Source/` | `Candy` | static lib | Core engine |
| `Candy_Editor/Source/` | `Candy_Editor` | binary (ConsoleApp) | Editor app |
| `Sandbox/Source/` | `Sandbox` | binary (ConsoleApp) | Test/demo (commented out by default) |

## Entry Point — Read This First

The engine **owns `main()`** in `Candy/Source/Candy/Core/EntryPoint.h`. Consumer apps are **not** standalone executables; they define:

```cpp
// In Candy namespace
Candy::Application* Candy::CreateApplication() {
    return new MyApp();  // MyApp extends Candy::Application
}
```

In the constructor, push layers via `PushLayer()` / `PushOverlay()`. The engine calls `Run()`, which loops: `OnUpdate(Timestep)` → `OnImGuiRender()` → `Window::OnUpdate()`.

## Precompiled Header

- **PCH file:** `Candy/Source/CandyPCH.h` (includes `Base.h`, `Log.h`, `Instrumentor.h`, `<Windows.h>` with `NOMINMAX`).
- All engine `.cpp` files under `Candy/Source/` use the PCH.
- **Third-party exceptions (NoPCH):** stb_image, ImGuizmo (see `Candy/premake5.lua:57-58`).

## Code Conventions

- **Namespace:** `Candy::`.
- **Smart pointers:** `Scope<T>` = `std::unique_ptr<T>`, `Ref<T>` = `std::shared_ptr<T>`. Use `CreateScope<T>(args...)` / `CreateRef<T>(args...)` factories (`Core/Base.h`).
- **Event binding:** `CANDY_BIND_EVENT_FN(fn)` — expands to a generic lambda. Do **not** use `std::bind`.
- **Logging:** `CANDY_CORE_*` macros for engine internals, `CANDY_*` for client/editor code (`spdlog`-based).
- **Config defines:** `CANDY_DEBUG`, `CANDY_RELEASE`, `CANDY_DIST` — set by premake config filters.
- **Windowing:** Uses GLFW (forked). `Candy::Window` is abstract; concrete impl is `WindowsWindow` in `Platform/Windows/`.
- **YAML:** Uses yaml-cpp (forked). Scene files use `.candy` extension.
- **Main include for consumers:** `#include "Candy.h"` (aggregates all public engine headers).

## Architecture Notes

### Layer System
`Application` owns a `LayerStack`. Layers hook into `OnAttach`, `OnDetach`, `OnUpdate(Timestep)`, `OnImGuiRender()`, `OnEvent(Event&)`. Overlays (pushed via `PushOverlay`) sit at the top of the stack — `ImGuiLayer` is always an overlay. Events propagate top-down (LIFO, reverse iterator).

### Renderer Backend
Abstracted behind virtual interfaces (`RendererAPI`, `Shader`, `Texture`, `Framebuffer`, etc.) with static `Create()` factories. The only current backend is OpenGL (`Platform/OpenGL/`). `RendererAPI::API` enum has only `OpenGL` today. Use `RenderCommand` (static) for draw calls — it delegates to the active backend singleton. `Renderer2D` provides batched 2D primitives (quads, circles, lines, sprites).

### ECS (Entity-Component-System)
Uses **EnTT** (`entt::registry`). `Scene` owns the registry and a Box2D `b2World`. `Entity` is a thin handle wrapping `entt::entity + Scene*`. Components are POD structs in `Scene/Components.h`:
- `IDComponent`, `TagComponent`, `TransformComponent`
- `SpriteRendererComponent`, `CircleRendererComponent`
- `CameraComponent`, `NativeScriptComponent`
- `Rigidbody2DComponent`, `BoxCollider2DComponent`, `CircleCollider2DComponent`

Scene serialization uses yaml-cpp via `SceneSerializer` (`.candy` files). Physics runtime bodies/fixtures stored as `void*` in components.

### Editor
`Candy_Editor` is a `ConsoleApp` that links `Candy`. Uses the same entry point pattern. `EditorLayer` manages viewport, gizmos (ImGuizmo), scene hierarchy, and content browser panels.

### ImGui Integration
- Core imgui is a separate static lib (`Imgui`).
- Backends (`imgui_impl_opengl3`, `imgui_impl_glfw`) are compiled inside `Candy` via `Imgui/ImguiBuild.cpp` with `IMGUI_IMPL_OPENGL_LOADER_GLAD`.
- `ImGuiLayer` is always pushed as an overlay in `Application` constructor.

### Platform Abstraction
Platform-specific code lives in `Candy/Source/Platform/`:
- `Platform/OpenGL/` — OpenGL backend implementations (buffers, shaders, textures, framebuffers, etc.)
- `Platform/Windows/` — Windows window, input, file dialogs

### Profiling
`Candy/Debug/Instrumentor.h` provides Chrome-tracing-compatible profiling. Use `CANDY_PROFILE_FUNCTION()` for RAII scoped timers. Outputs JSON files loadable in `chrome://tracing`.

## Adding New Source Files

1. Add `.cpp`/`.h` under `Candy/Source/`, `Candy_Editor/Source/`, or `Sandbox/Source/` — they're globbed by premake5 via `"Source/**.cpp"` and `"Source/**.h"`.
2. Headers under `Candy/Source/` are available via the implicit include dir `"Source"`.
3. When adding new vendored third-party, add explicit file patterns and `flags { "NoPCH" }` in the appropriate `premake5.lua`.

## Design Principles

- **Abstract backend, single impl:** Renderer API is abstract (virtual interfaces), but only OpenGL exists. The abstraction exists for future backends, not current flexibility.
- **Engine owns the loop:** `Application::Run()` drives the game loop — consumers only define layers.
- **Static singletons for services:** `Renderer`, `Renderer2D`, `RenderCommand`, `Input` are fully static classes (no instances).
- **Scene-centric ECS:** All entity/component logic flows through `Scene`. Systems are not standalone — they're called from `Scene::OnUpdate*` methods.
- **Editor as a consumer:** The editor (`Candy_Editor`) uses the same public API as any game app (`Application`, `Layer`, `Scene`, `Entity`, `Components`).
