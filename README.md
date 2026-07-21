# CandyEngine

<img src = "./Resources/Logo/Candy_Log.png" style = "zoom:75%" align = center />

CandyEngine 是一个使用 C++20 编写的 2D 游戏引擎，内置编辑器，采用 ECS（基于 EnTT）架构。当前架构受 [The Cherno](https://github.com/TheCherno) 的 [Hazel Engine](https://github.com/TheCherno/Hazel) 启发并以此为起点构建，设计理念进一步向 Godot 和 Unreal Engine 看齐——引擎与编辑器一体化、场景即数据、ECS 只是实现手段而非目的。

### 快速开始（用 git 生成 .sln）

本项目使用 `premake5.lua` 作为唯一真实的构建来源，通过脚本生成 Visual Studio 的 `.sln` / `.vcxproj`（这些文件均被 gitignore，不入库）。

```bash
# 1. 克隆仓库（务必带 --recursive 拉取第三方子模块）
git clone --recursive https://github.com/xltangcs/CandyEngine.git
cd CandyEngine

# 2. 生成 Visual Studio 工程（premake5 → .sln / .vcxproj）
.\Scripts\GenerateProjects.bat

# 3. 使用 MSBuild 构建（或直接在 Visual Studio 中打开 CandyEngine.sln）
msbuild CandyEngine.sln /p:Configuration=Debug

# 4. 运行编辑器
.\bin\Debug-windows-x86_64\CandyEditor\CandyEditor.exe
```

> **如果克隆时漏掉了子模块**，可以事后补救：
> ```bash
> git submodule update --init --recursive
> .\Scripts\GenerateProjects.bat
> ```

构建配置对应宏：`Debug` → `CANDY_DEBUG`，`Release` → `CANDY_RELEASE`，`Dist` → `CANDY_DIST`。

### 已实现的功能

| 模块 | 说明 |
|------|------|
| **构建系统** | premake5 生成工程，支持 Debug / Release / Dist 三套配置 |
| **Layer Stack** | 普通图层 + 叠加层（ImGui 始终为叠加层），事件 LIFO 反向传播 |
| **ECS** | 基于 EnTT，`Scene` 持有 `entt::registry`，`Entity = entt::entity + Scene*` |
| **渲染** | OpenGL 后端，`RendererAPI` 抽象接口；`Renderer2D` 批量绘制四边形 / 圆 / 线 / 精灵 |
| **物理** | Box2D 集成：`Rigidbody2DComponent`（Static/Dynamic/Kinematic）、`BoxCollider2DComponent`、`CircleCollider2DComponent`，仅在 Play / Simulate 运行 |
| **原生脚本** | C++ `ScriptableEntity` 基类 + `NativeScriptComponent::Bind<T>()` |
| **Python 脚本** | pybind11 已接入，提供 `candy.ScriptObject`（`on_construct` / `on_tick` / `on_destroy` 生命周期）；示例项目 JumpGame 即纯 Python 实现 |
| **编辑器** | Scene Hierarchy、Content Browser、Project Settings、Editor Settings |
| **3 态场景模式** | Edit（直接编辑，无物理）/ Play（深拷贝场景，物理 + 原生脚本）/ Simulate（深拷贝，仅物理，编辑器摄像机） |
| **场景序列化** | `.candy` 文件（yaml-cpp），可读、可版本控制、可合并 |
| **虚拟文件系统** | VFS（`VFS://` 协议 + Pak 打包） |
| **音频** | miniaudio 集成 |
| **UI 系统** | `UITextBlockComponent` / `UIButtonComponent` 等 |
| **日志** | spdlog 封装（`CANDY_CORE_*` / `CANDY_*`） |

**示例项目**
- **JumpGame**：纯 Python 脚本实现的跳跃躲避类游戏 Demo，位于 `JumpGame/`。

### 简单的未来计划

- **场景图 / 节点层次**：引入类 Godot 的节点层次结构，让 Entity 之间具备父子变换关系。
- **Python 脚本落地**：将 pybind11 骨架真正接入引擎运行时，支持在编辑器中热重载 Python 逻辑。
- **资源管线**：实现类 Godot / UE 的资源导入与管理系统（Content Browser 增强、`.import` 流程）。
- **场景继承与实例化**：让 `.candy` 场景支持继承与实例化。
- **渲染后端扩展**：在 `RendererAPI` 抽象之上增加 Vulkan / Metal / D3D 后端。
- **可视化脚本**：类 UE Blueprint 的节点化脚本系统。
- **SerializeRuntime**：补全运行时场景的序列化能力。

### 项目结构

| 目录 | 目标 | 类型 | 用途 |
|------|------|------|------|
| `Candy/Source/` | `Candy` | 静态库 | 引擎核心 |
| `CandyEditor/Source/` | `CandyEditor` | ConsoleApp | 编辑器（链接 `Candy`） |
| `JumpGame/` | — | 示例 | Python 脚本游戏 Demo |
| `ThirdParty/` | — | 依赖 | 第三方库（含 premake5、pybind11 等子模块） |

### 致谢

- 本引擎以 [The Cherno](https://github.com/TheCherno) 的 [Hazel Engine](https://github.com/TheCherno/Hazel) 为起点与灵感来源，感谢 Yan Chernikov 的系列教程为引擎架构奠定基础。
- 设计目标进一步参考了 [Godot](https://godotengine.org/) 与 [Unreal Engine](https://www.unrealengine.com/) 的编辑器与场景组织理念。

### License

本项目采用 **Apache License 2.0** 开源协议（与 Hazel 保持一致），包含明确的专利授权与修改标注要求。详见仓库根目录的 `LICENSE` 文件。

主要第三方依赖及其许可：

| 依赖 | 许可 |
|------|------|
| GLFW | zlib/libpng |
| GLAD | MIT |
| EnTT | MIT |
| imgui | MIT |
| ImGuizmo | MIT |
| glm | MIT (Happy Bunny) |
| spdlog | MIT |
| yaml-cpp | MIT |
| Box2D | MIT |
| pybind11 | BSD 3-Clause |
| stb_image | MIT / Public Domain |
| miniaudio | MIT / Public Domain |
| Open Sans (字体) | SIL Open Font License 1.1 |
