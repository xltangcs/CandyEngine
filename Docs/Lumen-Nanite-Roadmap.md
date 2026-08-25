# CandyEngine「Lumen-like / Nanite-like」复刻计划

> 版本：v1.0（2026-08-20）
> 基线 commit：`7d922c78`
> 结论：**Nanite 比 Lumen 更难**；建议路径：阶段 0 共同地基 → Lumen 线优先 → Nanite 线视资产管线成熟度再启

---

## 1. 背景与结论

### 1.1 难度对比总览

| 维度 | Lumen（实时 GI） | Nanite（虚拟化几何） |
|---|---|---|
| 现有可复用种子 | compute dispatch / UAV / barrier 全链路已被 IBL 烘焙验证（`D3D12TextureCubemap.cpp:39-366`）；HDR 中间目标、全局曝光已打通 | **几乎为零** |
| 编译器门槛 | SM5.0 可起步，SM6 更好 | **硬性要求 DXC + SM6.5**（当前 D3DCompile 锁定 `vs_5_0/ps_5_0/cs_5_0`，见 `D3D12Device.cpp:481-545`） |
| 缺失的 RHI 原语 | 3D 纹理、GBuffer 多附件、async queue、bindless | mesh/amplification stage、`ExecuteIndirect`、bindless、FL 12_1+ 特性检测、软件光栅路径——**整类缺失**（`ShaderStage` 枚举无 Mesh，`RHITypes.h:34-43`；PSO 只填 VS/PS，`D3D12Device.cpp:1081-1084`） |
| 算法复杂度 | 极高（surface cache / SDF / tracing / 降噪） | 极高（cluster DAG / GPU 级联剔除 / 软光栅 / 分页流送） |
| 工作量粗估（单人） | 地基后 3~6 个月 | 地基后 4~8 个月 |

**核心判断**：Lumen 是"扩展已验证路径"（compute 管线已被 IBL 烘焙验证），Nanite 是"先重做半层 RHI 再谈算法"。两者算法都是深水区，但 Nanite 还欠着整类底层能力。

### 1.2 现状基线（关键缺口，调研证据）

1. **每帧全同步**：`SceneRenderer.cpp:1165`、`GameFrameRenderer.cpp:171` 每帧 `Submit` 后 `WaitIdle()`；单命令队列、单命令缓冲、无多帧 in-flight
2. **无阴影**：`Components.h:200` `CastShadows = false; // reserved`；无 shadow map 任何实现
3. **无 GBuffer**：纯前向，HDR 中间目标仅 color + entity-id（R32Sint）两附件
4. **无 bindless**：D3D12 单 CBV_SRV_UAV 共享堆 2048 槽线性分配（`D3D12Device.cpp:346`），无 GPU 可见描述符数组
5. **无 StructuredBuffer / 间接绘制**：`ResourceUsage::Indirect` 位存在但全仓 0 处使用；无 `ExecuteIndirect` / `DispatchIndirect`
6. **无 3D 纹理**：`TextureType` 仅 Texture2D/Cubemap（`RHITypes.h:153-157`）——Lumen mesh SDF 体素化硬缺口
7. **无多线程**：全引擎单线程，无 job system；`SceneSerializer` 同步阻塞导入 glTF
8. **无 LOD / meshlet / 实例化**：逐实体每 submesh 一次 `DrawIndexed`（`SceneRenderer.cpp:1172-1292`），instanceCount 恒为 1；CPU 仅做 AABB 视锥剔除
9. **GPU 内存**：每资源独立 committed resource；`RHIMemoryAllocator`（`Runtime/RHI/Shared/RHIMemoryAllocator.h`）已写好但未接线
10. **资源管线空白**：无 asset importer / registry / 二进制资产 / streaming；mesh 运行时解析 glTF（cgltf），无 meshoptimizer
11. **无场景层次**：`TransformComponent` 无 Parent/Children，无场景图

### 1.3 总路线

```
阶段 0（共同地基，6~10 周）
   ├──→ Lumen 线（3~6 个月）   ← 推荐优先
   └──→ Nanite 线（4~8 个月）   ← 视资产管线成熟度再启
```

### 1.4 "复刻"的定义

**X-like 教学级**：复刻架构思想（SDF 追踪 + surface cache 分层 / cluster 虚拟化 + GPU 剔除），规模与画质按 CandyEngine 体量裁剪，不对标 UE5 的性能数字。理由：

- 单开发者投入周期以"月"计，逐像素对标 UE5 范围必然失控
- 引擎当前连阴影/GBuffer/父子变换都没有，直接对标 UE5 数字无意义
- 每个里程碑以"可演示 demo"验收，而非以"与 UE5 对比"验收

---

## 2. 阶段 0：共同地基（6~10 周）

> 原则：每项独立可验收、独立可提交；全部完成后引擎即达"现代渲染器"水位。两条线都绕不开，先做稳赚不亏。

### 0.1 DXC 替换 D3DCompile（约 1.5 周）

- **目的**：解锁 SM6.x 编译（mesh shader 必须 SM6.5；Lumen 需要 wave intrinsics / SM6.6 bindless）
- **改动**：
  - `D3D12Device.cpp:481-545` 编译路径换成 `dxcompiler.dll`（`IDxcCompiler3`）
  - shader 目标升级 `vs_6_0/ps_6_0/cs_6_0`，保留 SM5.0 回退宏
  - `RHIShaderLibrary` 哈希 key 加入编译目标维度（`Runtime/RHI/Shared/RHIShaderLibrary.h`）
  - `ShaderStage` 枚举预留 Mesh/Amplification/Raygen 占位（`RHITypes.h:34-43`，本轮不接线）
- **新增**：`Candy/ThirdParty/dxc`（dll + 头文件，premake 加 `NoPCH` flag，dll 拷贝到输出目录）
- **验收**：现有 6 个 HLSL（PBR/IBLBake/Tonemap 等）以 SM6.0 编译，画面与 SM5.0 逐像素一致

### 0.2 干掉每帧 WaitIdle：帧并行（约 2 周）

- **改动**：
  - `RHICommandQueue`（`Runtime/RHI/RHICommandQueue.h`）增加 `SignalFence / WaitFence`
  - `RHIDevice` 持有 3 帧 in-flight 环形上下文（fence 轮转）
  - 动态 CB 改 ring buffer（现有 per-draw 256B slice 机制保留，按帧索引分桶）
  - `SceneRenderer.cpp:1165`、`GameFrameRenderer.cpp:171` 的 `WaitIdle` 改为 fence N-2 等待
  - 资源释放改帧末延迟回收队列
  - 编辑器拾取 `ReadPixel`（`EditorLayer.cpp:758-794`）改为读 N-1 帧，消除点击 stall
- **验收**：帧率提升可测；RenderDoc 抓帧无资源 hazard；编辑器拾取功能正常

### 0.3 StructuredBuffer + ExecuteIndirect（约 1 周）

- **改动**：
  - `RHITypes.h` `ResourceUsage` 启用 `StorageBuffer` 位；`RHIBuffer` 增加 stride/counter 描述
  - `RHICommandBuffer`（`Runtime/RHI/RHICommandBuffer.h`）增加 `ExecuteIndirect(sig, argsBuffer, count)`、`DispatchIndirect`
  - D3D12：`ID3D12CommandSignature`（indexed draw / dispatch 两种）；root signature 增 CBV_SRV_UAV 表项
- **自测**：compute 生成 draw 参数 → indirect 执行（如程序化草地方块 demo）
- **验收**：indirect 绘制与等价直接绘制画面一致

### 0.4 Bindless 描述符（约 1.5 周）

- **改动**：
  - `RHIDescriptorSetManager`（`Runtime/RHI/Shared/RHIDescriptorSetManager.h`）升级为 GPU 可见大堆：单 CBV_SRV_UAV 堆扩容 + 索引传参（SM6.6 `ResourceDescriptorHeap` 风格）
  - **废弃硬编码槽位常量**：纹理表 / FBO SRV / ImGui 双上下文 / 纹理 SRV 全部走 `AllocateRange` 永久区 + 动态区双层
- **验收**：shader 内可索引访问 ≥4096 张纹理；现有 4-SRV / 3-SRV 固定表路径全部迁移完毕

### 0.5 接线 RHIMemoryAllocator（约 1 周）

- **改动**：`D3D12Buffer.cpp:78,114`、`D3D12Texture.cpp:117,202,352`、`D3D12Framebuffer.cpp` 的 `CreateCommittedResource` 替换为 heap + placed resource 子分配；上传堆走线性 arena
- **验收**：`RHIMemoryAllocator` 自带统计面板显示块合并正常；小资源数量 ×100 时分配耗时下降

### 0.6 最小 Job System（约 1 周）

- **新增**：`Runtime/Core/JobSystem.{h,cpp}`——工作线程池 + 有依赖的任务提交（类 UE `FTaskGraph` 极简版）
- **接入**：glTF 解析、mip / IBL 烘焙挪出主线程；`SceneSerializer` 加载改异步 + 进度回调
- **验收**：打开含 10 个 mesh 的场景主线程零阻塞（帧时间无尖峰）

### 0.7 阴影贴图（约 1.5 周）

- **改动**：
  - `LightComponent::CastShadows`（`Components.h:182-204`）落地
  - 方向光 CSM（3 级联）+ spot 单张 + point cube（点光源可后置）
  - RHI：深度附件 comparison sampler、`RHIFormat::D32Float` shadow target
  - `PBR.hlsl` 增加 shadow 采样项；`SceneRenderer` 场景 pass 前插 shadow pass
- **验收**：编辑器开关 `CastShadows` 实时生效；CSM 级联可视调试模式

### 阶段 0 建议执行顺序

```
0.1 DXC → 0.2 帧并行 → 0.7 阴影 → 0.4 bindless → 0.3 SBO/Indirect → 0.5 内存 → 0.6 Job
```

理由：DXC 是 Nanite 硬前置；帧并行收益立竿见影；阴影独立价值最高，且是 Lumen 前必须存在的直接光基准。

---

## 3. Lumen 线：Lumen-like 实时 GI（阶段 0 后 3~6 个月）

> 参考架构：Lumen 的分层 GI（SDF 追踪求交 + surface cache 辐射度回采 + 最终汇聚降噪）。本线按"可裁剪"设计：每阶段结束都有一帧可见成果。

### L1 GBuffer（2 周）

- **内容**：场景 pass 扩为 MRT：albedo+metallic / normal+roughness / depth，entity-id 保留
- **关键改动**：
  - HDR 中间目标附件位扩展（`GameFrameRenderer.cpp:47-55` `EnsureHDRTarget`）
  - `PBR.hlsl` 拆分"材质采样"与"光照计算"；`SceneRenderer` 渲染 pass 描述扩展
- **验收**：各 GBuffer 通道可视化调试视图正确

### L2 SSGI 过渡版（3 周）—— **里程碑 M1**

- **内容**：屏幕空间 GI：half-res ray march（depth+normal）+ 时域累积 + 双边上采样
- **关键改动**：首个全屏 compute tracing pass；接入 tonemap 前的 HDR 链路（`GameFrameRenderer.cpp` 编排）
- **验收**：**肉眼可见间接光**（如白墙被红球染色）；作为 L4 的渲染管线预演

### L3 Mesh SDF 烘焙（3 周）

- **内容**：RHI 加 `TextureType::Texture3D`；导入时 compute 体素化每 mesh 的 SDF，存为资产
- **关键改动**：`MeshImporter` 后处理步骤（`Runtime/Asset/MeshImporter.cpp`）；SDF 资产进 .pak / 缓存目录；烘焙任务走 Job System
- **验收**：SDF 切片可视化正确；与 mesh 原始几何误差在阈值内

### L4 Global SDF 追踪（4 周）—— **里程碑 M2**

- **内容**：场景 SDF clipmap 合成；cone trace 求交；辐射度先用 IBL / emissive 近似注入（**跳过 surface cache**）
- **关键改动**：简化版 Lumen 核心 pass 链（合成 → 追踪 → 注入 → 汇聚）
- **验收**：**动态物体移动实时改变间接光**（球移动时墙面染色跟随）

### L5 Surface Cache（6 周，最难，可裁剪）

- **内容**：mesh card capture（导入时烘 albedo/normal/emissive 到 atlas）；trace 命中后回采 atlas 取辐射度
- **关键改动**：atlas 分配器（依赖 0.4 bindless）；card 可见性更新
- **验收**：间接光带材质颜色（而非 IBL 近似）
- **裁剪策略**：L5 是 Lumen 真正的工作量黑洞。若超期，降级为"L4 + emissive 注入"收束，L5 拆成独立后续课题

### L6 Final gather + 降噪（3 周）

- **内容**：probe 化空间复用 + TAA 协同 + 时域滤波
- **关键改动**：与既有 exposure / tonemap 链路整合
- **验收**：demo 场景 1080p 中端 GPU 稳定 30fps

---

## 4. Nanite 线：Nanite-like 虚拟化几何（阶段 0 后 4~8 个月）

> 参考架构：离线 meshlet + cluster DAG 简化层次 → GPU 两级剔除（instance + cluster）→ 硬件光栅 + 小三角形软件光栅 → 分页流送。

### N1 离线 mesh 处理（3 周）—— **里程碑 N-M1**

- **内容**：引入 meshoptimizer；导入时构建 meshlet（~64 tri）+ cluster 简化 DAG；烘焙为二进制资产
- **关键改动**：**顺带补齐引擎空白的资产导入管线**（asset importer + 缓存目录）；`MeshData.h` / `StaticMeshResource.h` 增加 cluster 结构
- **验收**：任意 glTF 导入即得 cluster 层次；DAG 深度可视化

### N2 Mesh Shader 管线（3 周）

- **内容**：`ShaderStage` 接线 AS/MS；PSO 描述、root signature、D3D12 `CheckFeatureSupport`（mesh shader tier）；无 MS 硬件时 compute 模拟回退
- **关键改动**：`D3D12Device.cpp` PSO 创建分支；SM6.5 编译目标（依赖 0.1）
- **验收**：meshlet 线框模式渲染与 VS 路径画面一致

### N3 GPU 剔除 + 间接绘制（4 周）—— **里程碑 N-M2**

- **内容**：compute 两级剔除：instance（视锥 + AABB）→ cluster（视锥 + HiZ 遮挡回读，用上帧深度金字塔）；`ExecuteIndirect` 提交
- **关键改动**：HiZ 金字塔 compute 生成；依赖 0.3 / 0.4
- **验收**：**百万三角场景 CPU draw call ≈ 1**；帧率不受三角形数量线性影响

### N4 运行时 LOD（4 周）

- **内容**：cluster DAG 屏幕空间误差驱动的 LOD 选择（GPU 上遍历/标记）；父子 cluster 一致性（边界锁定防裂缝）
- **关键改动**：最难的算法部分；建议先 CPU 验证再搬 GPU
- **验收**：相机推拉 LOD 无缝切换，无裂缝、无闪烁

### N5 软件光栅（6 周，招牌也最硬）

- **内容**：小三角（<阈值像素）走 compute visibility buffer（原子写 64bit depth+clusterID）；大三角走硬件 MS 光栅；材质 resolve pass
- **关键改动**：需要 visibility buffer → GBuffer 化改造（与 L1 GBuffer 复用）
- **验收**：密集植被 / 岩石场景性能超越纯硬件光栅路径

### N6 分页流送（4 周）

- **内容**：cluster 页（128KB 粒度）按需加载 / 驱逐，LRU + 预取
- **关键改动**：依赖 0.5 / 0.6；`.pak`（`Runtime/Core/PakFile.h`）扩展为压缩分页格式
- **验收**：超显存规模场景可流畅漫游，无卡顿加载

### Nanite 线前置提醒

N 线启动前建议先补引擎两块自身短板（顺序可对调）：

1. **父子变换 / 场景层次**：当前 `TransformComponent` 无 Parent，大场景实例复用全靠复制实体
2. **资产导入管线**：N1 会倒逼它诞生——也可以反过来，先做资产管线再做 N1

---

## 5. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 阶段 0 触碰面广（渲染循环 / 资源生命周期），回归风险高 | 编辑器 / 游戏全链路 | 每项独立验收 + RenderDoc 抓帧对比；0.2 之前给 `CandyGame` 录基准视频 |
| DXC 分发（dll 体积 / 路径） | 编辑器部署 | dll 进 `ThirdParty/dxc/bin`，premake 拷贝到输出目录 |
| SM6.6 bindless 需要较新驱动 / 硬件 | 开发机兼容性 | 特性检测 + 固定表回退路径（只保 D3D12；OpenGL/Vulkan 维持现状不动） |
| L4/L5 与 N4/N5 是算法深水区，估算不确定性大 | 进度 | 每个深水区阶段先出 1 周 spike（原型验证）再正式排期 |
| 单开发者投入周期以"月"计 | 动力 / 长期维护 | 里程碑 M1 / N-M2 均为可演示 demo，按里程碑验收而非按周；各阶段向后兼容独立提交 |
| OpenGL / Vulkan 后端被抛下 | 双后端承诺 | 阶段 0 明确：底层升级（bindless / DXC / 帧并行）只保 D3D12，OpenGL/Vulkan 维持现状并标注 FROZEN 语义 |

---

## 6. 里程碑总览

| 里程碑 | 内容 | 可演示成果 | 预计累计周期 |
|---|---|---|---|
| M0 | 阶段 0 全部完成 | 现代渲染器基线（SM6、帧并行、阴影、bindless、间接绘制） | 6~10 周 |
| M1 | L2 SSGI | 屏幕空间间接光 | M0 + ~6 周 |
| M2 | L4 Global SDF 追踪 | 动态物体实时改变间接光 | M1 + ~7 周 |
| M3 | L6 完成 | 完整 Lumen-like GI demo（含降噪） | M2 + ~9 周（L5 裁剪则 ~3 周） |
| N-M1 | N1 离线 cluster 化 | 任意 glTF 导入即得 meshlet 层次 | M0 + ~3 周 |
| N-M2 | N3 GPU 剔除 + indirect | 百万三角单 draw call | N-M1 + ~7 周 |
| N-M3 | N6 完成 | 超显存规模场景流畅漫游 | N-M2 + ~14 周 |

---

## 7. 推荐执行顺序

```
0.1 DXC → 0.2 帧并行 → 0.7 阴影 → 0.4 bindless → 0.3 SBO/Indirect → 0.5 内存 → 0.6 Job
   → L1 GBuffer → L2 SSGI（首个可见 GI 效果）
   → L3 → L4（Lumen 核心 demo）
   →（评估：继续 L5/L6，或转 N1 补资产管线，或做场景层次）
```

阶段 0 内部顺序按"风险前置 + 成就感前置"排列：DXC 是硬前置，帧并行收益立竿见影，阴影独立价值最高。

---

## 8. 附录：现状代码基证据索引

| 能力 | 状态 | 位置 |
|---|---|---|
| Compute（D3D12-only） | 可用，仅 IBL 烘焙在用 | `RHIPipelineState.h:128-136`、`RHIDevice.h:171-178`、`D3D12Device.cpp:774-894`、`D3D12TextureCubemap.cpp:39-366` |
| Ray tracing | 无 | 设备 FL 12_0（`D3D12Device.cpp:262,286`）；全仓 0 命中 |
| Mesh shader | 无 | `ShaderStage` 无 Mesh（`RHITypes.h:34-43`）；PSO 仅 VS/PS（`D3D12Device.cpp:1081-1084`） |
| Shader 编译 | D3DCompile / DXBC / SM5.0 | `D3D12Device.cpp:481-545`；无 DXC 命中 |
| 纹理格式 | 枚举全、映射窄；无 3D 纹理 | `D3D12Texture.cpp:17-31`、`D3D12Framebuffer.cpp:55-59` |
| 描述符 | 2048 槽线性分配 | `D3D12Device.cpp:313-346`、`RHIDescriptorSetManager.h` |
| 渲染流程 | 前向 + HDR + tonemap | `SceneRenderer.{h,cpp}`、`GameFrameRenderer.{h,cpp}`、`Tonemap.hlsl` |
| 阴影 | 未实现 | `Components.h:200` |
| 剔除 | CPU AABB 视锥 | `SceneRenderer.cpp:115-160,963-992` |
| 提交 | 每实体每 submesh 一次 DrawIndexed | `SceneRenderer.cpp:1172-1292` |
| 帧同步 | 每帧 WaitIdle | `SceneRenderer.cpp:1164-1165`、`D3D12Device.cpp:1312-1316` |
| Mesh 导入 | cgltf，运行时解析，无 LOD | `Runtime/Asset/MeshImporter.cpp`、`MeshData.h` |
| 资源管线 | 无 importer/registry/二进制资产/streaming | `ContentBrowserPanel.cpp`、`PakFile.h` |
| GPU 内存 | committed resource | `D3D12Buffer.cpp:78,114`、`D3D12Texture.cpp:117,202,352` |
| 多线程 | 无 job system | 全仓 grep 仅日志/采样线程 |
| 场景层次 | 无 Parent/Children | `Components.h` `TransformComponent` |
| 内存分配器 | 写好未接线 | `Runtime/RHI/Shared/RHIMemoryAllocator.h` |
