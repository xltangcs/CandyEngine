# AGENTS.md — CandyEngine

**语言：AI 回答请使用中文**

## Build

```pwsh
git submodule update --init --recursive
.\Scripts\GenerateProjects.bat    # premake5 → .sln/.vcxproj (均 gitignored)
msbuild CandyEngine.sln /p:Configuration=Debug
.\bin\Debug-windows-x86_64\CandyEditor\CandyEditor.exe
```

- `premake5.lua` 是唯一真实构建来源，**不要手动编辑生成的 .sln/.vcxproj**
- 配置: `Debug`/`Release`/`Dist` → `CANDY_DEBUG`/`CANDY_RELEASE`/`CANDY_DIST`
- Sandbox 默认注释在根 `premake5.lua:40`

## Design Philosophy — 参考 Godot & UE

本引擎虽当前架构接近 Hazel Engine（The Cherno 教程），但**设计目标应向 Godot 和 UE 靠拢**：

| 概念 | Godot | UE | CandyEngine（当前） | 目标方向 |
|------|-------|----|--------------------|---------|
| 场景组织 | SceneTree + Node | Actor + Component | `Scene` 拥有 `entt::registry` | 引入节点层次/场景图 |
| 脚本 | GDScript / C# | Blueprint / C++ | `NativeScriptComponent::Bind<T>()` | 类 Godot 节点附加脚本 / 类 UE Blueprint 可视化脚本 |
| 编辑器 | 内嵌编辑器 | 内嵌编辑器 | `EditorLayer` 管理 Edit/Play/Simulate | 保持 3 态场景模式（类似 UE 的 PIE），增强 Play-in-Editor |
| 场景文件 | .tscn | .umap | `.candy` (yaml-cpp) | 可读格式，逐步引入场景继承/实例化 |
| 资源管理 | .import | Content Browser | 尚无资源管线 | 需实现类 Godot/UE 的资源导入和管理系统 |

**关键理念：**
- **保持引擎与编辑器的一体化设计**（像 Godot/UE 一样，编辑器是引擎的增强模式）
- **场景即数据**：`.candy` 场景文件应可版本控制、可合并
- **ECS 是实现手段，不是目的**：用户面对的是 Entity + Component API，而非裸 entt

## Project Structure

| 目录 | 目标 | 类型 | 用途 |
|---|---|---|---|
| `Candy/Source/` | `Candy` | 静态库 | 引擎核心 |
| `CandyEditor/Source/` | `CandyEditor` | ConsoleApp | 编辑器（链接 `Candy`） |
| `Sandbox/Source/` | `Sandbox` | ConsoleApp | 测试 demo（默认不构建） |

## Entry Point

引擎拥有 `main()`（`Candy/Source/Candy/Core/EntryPoint.h`）。应用只需：

```cpp
// 在 Candy 命名空间下
Candy::Application* Candy::CreateApplication() {
    return new MyApp();  // 继承 Candy::Application
}
```

构造函数中 `PushLayer()` / `PushOverlay()` 注册图层，引擎调用 `Run()`：
`OnUpdate(Timestep)` → `OnImGuiRender()` → `Window::OnUpdate()`（swap + poll）

## Architecture Essentials

- **Layer stack**：普通图层 + 叠加层（ImGuiLayer 始终为叠加层）。事件反向传播（LIFO）
- **3 种场景状态**（`EditorLayer` 管理）：
  - **Edit**: 直接编辑 `m_EditorScene`，无物理
  - **Play**: `Scene::Copy(m_EditorScene)` → `OnRuntimeStart()` → 物理 + 原生脚本
  - **Simulate**: 深拷贝，仅物理，编辑器摄像机
- **渲染**: `RendererAPI` 抽象接口 + 静态单例 `RenderCommand` / `Renderer2D`（批量四边形/圆形/线条/精灵），当前仅 OpenGL 后端
- **ECS**: 使用 EnTT，`Scene` 拥有 `entt::registry` + `b2World`。`Entity` = `entt::entity + Scene*`。组件位于 `Scene/Components.h`
- **物理**: Box2D 集成（Rigidbody2D + Box/CircleCollider2D），运行时 body/fixture 存为 `void*`
- **原生脚本**: `ScriptableEntity` 基类 + `NativeScriptComponent::Bind<T>()`
- **Python 脚本**（计划中）：pybind11 已加入依赖但**引擎源码尚未使用**
- **序列化**: yaml-cpp，`.candy` 文件。`SerializeRuntime()` 尚未实现

## Code Conventions

- 命名空间 `Candy::`，PCH: `CandyPCH.h`（所有 `Candy/Source/` .cpp 使用）
- `Scope<T>` = `unique_ptr<T>`，`Ref<T>` = `shared_ptr<T>`，工厂 `CreateScope`/`CreateRef`
- `CANDY_BIND_EVENT_FN(fn)` 绑定事件（不要用 `std::bind`）
- 日志: `CANDY_CORE_*`（引擎内部） / `CANDY_*`（客户端/编辑器）
- 入口 include: `#include "Candy.h"`（聚合所有公共头）
- 新增源文件放到对应 `Source/` 下即可（premake 通过 `Source/**.cpp` glob 包含），新增第三方源需加 `NoPCH` flag
- 缩进使用 Tab，与现有 ~95% 文件保持一致
- 行尾使用 CRLF（回车+换行），与现有大部分文件保持一致

## Python 绑定现状

pybind11 子模块已添加（`Candy/ThirdParty/pybind11`），`IncludeDir` 和 premake includedirs 已配置，但引擎源码中**无任何 pybind11 使用代码**。这是一个预留骨架，等待实现。
