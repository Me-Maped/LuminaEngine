# Lumina Engine 渲染架构

本文描述 Lumina Engine 渲染器的真实实现：一个 Vulkan-only、以 mesh shader 与 GPU-driven meshlet culling 为基础的渲染器。所有结论均来自仓库中的真实源码，并在每条结论后用行内代码给出对应路径。

> **关于同步模型**：仓库中没有 RenderGraph 一类的自动依赖推导 / barrier 插入层。`grep -rn "RenderGraph"` 只在 `CONTRIBUTING.md` 的类型命名示例中命中一次，`FRenderGraph` 没有任何定义或使用。同步由调用方显式驱动：`RHI::CmdBarrier` 加上一组语义化 helper `RHI::Barriers::*`（`RasterToRead`、`RasterToRaster`、`TransferToTransfer`、`SceneToTransfer`、`TransferToShaders`、`TransferToCompute`、`ComputeToGeometry`、`ComputeToIndirect`）。README 中「自动资源追踪与 barrier 插入」在本仓库对应的是这组 helper，而不是自动推导；详见文末「已知差异与局限」。

## 1. 概览

渲染器只实现了 Vulkan 后端。

| 事实 | 源码 |
| --- | --- |
| `EBackend` 只有 `Vulkan` 是默认实现，`Metal`/`DX12` 标注为 `@TODO` | `Engine/Source/Runtime/Source/Renderer/RHI.h` |
| 目录中只有 `API/Vulkan/` 一个后端 | `Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp` |
| 要求 Vulkan 1.4（`VK_API_VERSION_1_4`） | `Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp` |
| 几何路径要求 `VK_EXT_mesh_shader`，设备选择时以 `EDeviceFeature::MeshShading` 强制 | `Engine/Source/Runtime/Source/Renderer/RHI.h`、`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp` |
| 最低显存 3840 MiB（`kMinimumVRAMMiB`），推荐 6144 MiB（`kRecommendedVRAMMiB`） | `Engine/Source/Runtime/Source/Renderer/RenderManager.cpp` |

`FRenderManager::Initialize` 是硬件底线的落点：它构造 `RHI::FDeviceDesc` 时写死 `RequiredFeatures = RHI::EDeviceFeature::MeshShading` 与 `MinDeviceLocalMemoryMiB`。

```cpp
// RenderManager.cpp: the renderer has no non-mesh fallback.
RHI::CreateDevice(RHI::FDeviceDesc
{
    .bValidation = bValidation,
    .bDebugUtils = bDebugUtils,
    .bHeadless   = false,
    // The scene renderer draws every meshlet through the mesh path; there is no fallback.
    .RequiredFeatures = RHI::EDeviceFeature::MeshShading,
    .MinDeviceLocalMemoryMiB = MinVRAMMiB,
    .PipelineCachePath = PipelineCachePath(),
});
```

`-ignoreminspec` 把最小值降为 0，`-minvram=N` 覆盖阈值，两者都只影响设备选择（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。

### 分层结构

```
应用 / 帧循环
  Core/Application/Application.cpp  主循环、输入泵、WaitForFrameSlot
  Core/Engine/Engine.cpp            Update 阶段、ExtractWorlds、FrameEnd
World / Scene（场景侧）
  World/Scene/RenderScene/          IRenderScene、RenderSceneFactory、MeshDrawCommand
  World/Scene/RenderScene/Default/  FDefaultSceneRenderer：一帧的 pass 编排
渲染器管理层（Renderer/）
  RenderManager.h/.cpp              FRenderManager、FScenePipelineCache、共享资源
  MaterialManager.h                 FMaterialManager、FMaterialCollectionManager
  TextureStreamingManager.h         纹理 mip residency 策略
  GPUProfiler.h / GPUSpan.h         计时与带边界的 GPU span
RHI 抽象层（Renderer/）
  RHI.h / RHICore.h                 公开类型与 handle；帧环、唯一提交入口、retire
  RHITexture.h / RHIUpload.h / RHIUtils.h  纹理辅助、staging 上传、screen pass
后端实现与错误处理
  API/Vulkan/VulkanRHI.cpp          约 7900 行的 Vulkan 实现
  ErrorHandling/Vulkan/             validation 回调、Aftermath、RGD、breadcrumbs
```

相关目录：`Engine/Source/Runtime/Source/Renderer/`、`Engine/Source/Runtime/Source/World/Scene/RenderScene/`、`Engine/Resources/Shaders/`。

## 2. RHI 分层

每个头文件有明确的职责边界，公开面与内部面是分开的。

| 头文件 | 职责 |
| --- | --- |
| `RHI.h` | API 中立类型：`EQueueType`、`EFormat` 之外的枚举、`FDeviceDesc`、`FRenderPassDesc`、`FPipelineH` 等 handle、`CreateDevice`/`Malloc`/`CreateTexture`/`CreateTextureHeap`/`CreateMeshShaderPipeline` |
| `RHIFwd.h` | 极小的前向声明，只有 `FShaderEntry` 与 `FShaderLibrary` |
| `RHICore.h` | 帧环与提交：`BeginFrame`、`Submit`、`GetQueueTimeline`、`AllocTransient`、`Retire*`、`GetGlobalHeap`；`EStockSampler` |
| `RHIInternal.h` | 只给 retire drain 用的立即销毁与原始 submit：`Internal::CreateDevice`、`Internal::DestroyNow`、`Internal::Submit` |
| `RHINative.h` | 原生句柄与插件扩展点，见下 |
| `RHIUpload.h` | staging 上传：`UploadBuffer`、`UploadTexture`、`FlushUploads`、`CancelTexture` |
| `RHITexture.h` | `FManagedTexture`、`Textures::Create/Recreate/CommitRecreate/Upload/Release` |
| `RHIUtils.h` | `BeginScreenPass`、`DrawFullscreen`、`FMipChain` |
| `Format.h` | `EFormat` 与 `ETextureSwizzle` |
| `RenderResource.h` | `FFormatInfo`、`ERHIShaderType`（序列化进 shader cache，值必须稳定） |

handle 都是 `THandle<T>`，因此在 CPU 侧只是可判空的弱引用：

```cpp
// RHI.h
using GPUPtr            = uint64;
using FPipelineH        = THandle<FPipeline>;
using FTextureH         = THandle<FTexture>;
using FTextureHeapH     = THandle<FTextureHeap>;
using FSemaphoreH       = THandle<FSemaphore>;
using FCmdListH         = THandle<FCommandList>;
using FSwapchainH       = THandle<FSwapchain>;
using FSurfaceH         = THandle<FSurface>;
using FQueryPoolH       = THandle<FQueryPool>;
```

`RHIInternal.h` 是明确不公开的边界：`Internal::Submit` 是原始队列提交，注释说明“Every public path routes through the queue timelines in RHICore.cpp”，因此外部只能走 `RHI::Submit`（`Engine/Source/Runtime/Source/Renderer/RHIInternal.h`、`Engine/Source/Runtime/Source/Renderer/RHICore.cpp`）。

`RHINative.h` 是插件/外部工具的扩展点：

- `GetNativeDeviceHandles()` 返回 `VkInstance`/`VkPhysicalDevice`/`VkDevice`/`VkQueue` 等原始句柄。
- `RegisterDeviceCreationRequest(const FDeviceCreationRequest&)` 允许插件在 `CreateDevice` 之前追加 instance extension、device extension 和 `VkDeviceCreateInfo::pNext`；`VulkanRHI.cpp` 在扩展收集阶段遍历 `GPendingDeviceRequests`。
- `AcquireSubmitLock`/`ReleaseSubmitLock` 及其 RAII 形式 `FScopedSubmitLock` 用来串行化外部提交与引擎提交。

```cpp
// RHINative.h
struct FDeviceCreationRequest
{
    TVector<const char*> InstanceExtensions;
    TVector<const char*> DeviceExtensions;
    void*                DeviceCreatePNext = nullptr;
};

RUNTIME_API void RegisterDeviceCreationRequest(const FDeviceCreationRequest& Request);

struct FScopedSubmitLock
{
    FScopedSubmitLock()  { AcquireSubmitLock(); }
    ~FScopedSubmitLock() { ReleaseSubmitLock(); }
    FScopedSubmitLock(const FScopedSubmitLock&) = delete;
};
```

## 3. 设备、队列与交换链

### 设备创建与选择

`RHI::CreateDevice` → `Internal::CreateDevice` → `SelectPhysicalDevice` 的过程是：创建 instance（validation 在这里决定）、枚举物理设备、按 `EvaluateDeviceSuitability` 逐个筛选、再按类型打分（独显 1000，集显 100），最后创建逻辑设备。

`EvaluateDeviceSuitability` 检查（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）：

- 非 headless 必须有 `VK_KHR_swapchain` 和 graphics queue family。
- 显存低于 `FDeviceDesc::MinDeviceLocalMemoryMiB` 时打 `bRejectedForMemory`。
- mesh shader：必须有 `VK_EXT_mesh_shader` 且 `meshShader` feature 为真；`maxMeshWorkGroupSize[0]`、`maxMeshWorkGroupInvocations`、`maxMeshOutputVertices`、`maxMeshOutputPrimitives` 至少要达到 `kMeshWorkGroupSize = 32`、`kMeshMaxOutputVertices = 64`、`kMeshMaxOutputPrimitives = 64`；mesh stage 必须支持 subgroup shuffle；subgroup 可能窄于 32 时，设备必须支持把 mesh stage 钉到 32（`subgroupSizeControl`）。不满足且调用方要求 `MeshShading` 时直接拒绝。

这些常量定义在 `Engine/Source/Runtime/Source/Renderer/RHI.h`：

```cpp
constexpr auto kMeshWorkGroupSize           = 32u;   // MESHLET_MESH_GROUP_SIZE (MeshletGeometry.slang)
constexpr auto kMeshMaxOutputVertices       = 64u;   // out vertices Verts[MESHLET_MAX_VERTICES]
constexpr auto kMeshMaxOutputPrimitives     = 64u;   // out indices Tris[MESHLET_MAX_TRIANGLES]
```

### 队列

`EQueueType` 有 `Graphics`、`Transfer`、`Compute`（`Engine/Source/Runtime/Source/Renderer/RHI.h`）。是否真的有独立队列由 `RHI::SupportsAsyncCompute()` / `SupportsAsyncTransfer()` 回答。后端用 `QueueLockIndex` 处理别名队列：多个类型可能映射到同一个 `VkQueue`，`QueueLockFor` 返回拥有该队列的锁，防止别名类型重复加锁（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）。

每个队列有一条 timeline semaphore，`RHI::GetQueueTimeline(Queue)` 用于等待。`Submit` 会返回本次提交信号到的值，等待方式就是 `WaitSemaphore(GetQueueTimeline(Queue), Value)`（`Engine/Source/Runtime/Source/Renderer/RHICore.h`）。

### 交换链与 resize

呈现由 `RHI::FSwapchainTarget` 独占（`Engine/Source/Runtime/Source/Renderer/SwapchainTarget.h`）：

| 方法 | 作用 |
| --- | --- |
| `Initialize(Surface, Extent)` | 接管 `FSurfaceH`；`Extent` 为 0 时把首次构建推迟到 `Resize` |
| `Resize(Extent)` | 只有与上次请求的 extent 不同才重建 |
| `Recreate()` | 以当前 extent 重建，用于 present mode 变化这类 extent 表达不了的情况 |
| `Acquire()` | 返回一个 invalid handle 表示本帧没有可绘制图像，重试已自动武装 |
| `BarrierToRender(CL)` | 把取到的图像转入可渲染布局，在第一个 pass 前记录 |
| `Present(CL)` / `PresentAsync(CL)` / `FinishPresent()` | 提交并呈现；被拒绝的 present 保留图像，所以宿主必须重建 |

present mode 是反射枚举 `EPresentMode`：`Immediate`（最低延迟、撕裂）、`Mailbox`（不撕裂、不锁帧）、`FIFO`（不撕裂、锁刷新率，永远支持）（`Engine/Source/Runtime/Source/Renderer/PresentMode.h`）。

resize 路径是异步的：`FWindow::OnWindowResized` → `FRenderManager::OnWindowResized` 只把 extent 打包进 `PendingResizeExtent`；下一帧 `FrameEnd` 里的 `ApplyPendingResize` 才调用 `SwapchainTarget.Resize` 并广播 `OnSwapchainResized`（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。`FSwapchainTarget` 里 `BuiltExtent` 记录实际建成的大小（surface 可能把它夹小），`RequestedExtent` 记录请求值。

## 4. 内存与资源

### VMA 与分配器

VMA 是唯一的显存分配后端：`vmaCreateAllocator` 在逻辑设备创建后调用，flags 包含 `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`，并按可用扩展加 memory priority / memory budget（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）。Vulkan loader、driver 与 VMA 自己的 host 分配全部走引擎分配器：`Vulkan::HostAllocator()` 返回的 `VkAllocationCallbacks` 转发到 `Memory::Malloc/Realloc/Free`（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanAllocator.h`、`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanAllocator.cpp`）。

`RHI::Malloc` 面向的是带 device address 的 buffer：小分配进入按 `EMemoryType` 分页的 backing block，页内再用 `VmaVirtualBlock` 子分配；大分配走 VMA dedicated allocation（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）。`EMemoryType` 是 `CPUWrite`、`CPURead`、`GPUOnly`，分别对应顺序写、随机读、仅 GPU 的 VMA 用法。统计通过 `GetGPUMemoryStats`、`GetGPUPoolStats` 暴露（`Engine/Source/Runtime/Source/Renderer/RHI.h`）。

### 资源句柄

| 类型 | 说明 | 源码 |
| --- | --- | --- |
| `FPipelineH` | pipeline handle | `Engine/Source/Runtime/Source/Renderer/RHI.h` |
| `FTextureH` | 原生纹理 handle | 同上 |
| `FTextureHeapH` | bindless descriptor heap handle | 同上 |
| `FSemaphoreH` | timeline semaphore | 同上 |
| `FManagedTexture` | `FTextureH` + bindless `SampledSlot`，`ResourceID()` 即 shader 里的纹理 ID | `Engine/Source/Runtime/Source/Renderer/RHITexture.h` |
| `FShaderH` | shader library 条目的弱、带代数 handle | `Engine/Source/Runtime/Source/Renderer/ShaderHandle.h` |
| `FGPUAllocation` | `Cpu`/`Gpu`/`Size` 的一次分配 | `Engine/Source/Runtime/Source/Renderer/RHI.h` |

```cpp
// RHITexture.h
struct FManagedTexture
{
    FTextureH Texture;
    uint32    SampledSlot = kInvalidHeapSlot;   // ResourceID for gTextures2D[]

    uint32 ResourceID() const { return SampledSlot; }
    bool   IsValid() const { return RHI::IsValid(Texture); }
};
```

### bindless descriptor heap

存在一个进程级全局 heap：`RHI::GetGlobalHeap()`，在 `RHICore.cpp` 的 `InitializeCore` 中按设备上限创建：`CreateTextureHeap(kMaxSampledTextureHeapSize, kMaxStorageTextureHeapSize, 64)`。heap 的写入/重指向/解绑通过 `HeapWriteTexture`、`HeapRepointTexture`、`HeapWriteSampler`、`HeapRewriteSampler`、`HeapUnbindTexture`、`HeapFreeTexture` 等完成；repoint 与 unbind 会等每个 frame slot 的 heap 副本在 `BeginFrame` 前空闲，避免飞行中的帧看到变化（`Engine/Source/Runtime/Source/Renderer/RHI.h`）。

### 延迟释放

RHI 的释放从不立即发生（`Shutdown` 除外）。`RHI::Retire(...)` 把释放项按当前 frame slot 入队，记录各队列当前的 timeline 值；下一次该 slot 的 `BeginFrame` 里 `DrainRetireQueue` 才在 GPU 越过该值时真正销毁（`Engine/Source/Runtime/Source/Renderer/RHICore.cpp`）。

```cpp
// RHICore.h
RUNTIME_API void Retire(const FGPUAllocation& Memory);
RUNTIME_API void Retire(FTextureH Texture);
RUNTIME_API void Retire(FPipelineH Pipeline);
```

其余资源类型（`FSemaphoreH`、`FTextureHeapH`、`FSwapchainH`、`FSurfaceH`、`FQueryPoolH`）各有同名重载，统一进 `PushRetire`。

渲染器侧还有一层 extract 生命周期的闸门：`RHI::FRenderReleaseQueue`。它有两道门，第一道是 extract 存活（没有任何仍在飞行、可能命名该资源的 extract），第二道才是 GPU 存活（走 `RHI::Retire`）。资源释放项 `FRenderRelease` 只有 `MaterialSlot`、`FManagedTexture`、`FGPUAllocation` 三个字段，因为 owner 那时已经销毁（`Engine/Source/Runtime/Source/Renderer/RenderRelease.h`）。

## 5. 帧编排

`FRenderManager` 的公开 API 与每帧顺序（`Engine/Source/Runtime/Source/Renderer/RenderManager.h`）：

| API | 时机与作用 |
| --- | --- |
| `WaitForFrameSlot()` | 在应用主循环顶部、输入泵之前调用；阻塞到本帧 slot 的 GPU 工作完成 |
| `FrameStart(UpdateContext)` | 后端 per-frame 初始化，目前是 ImGui `StartFrame` |
| `FrameEnd()` | 重建/呈现一帧：`FinishPresent`、`ApplyPendingResize`、`RenderWorlds`、acquire swapchain、record present pass、present |
| `SwapchainResized(FVector2)` | 广播 `OnSwapchainResized` |
| `RecreatePrimarySwapchain()` | vsync / present mode 变化时重建 |
| `GetSharedRenderResources()` / `GetScenePipelineCache()` | 跨世界共享的纹理与 pipeline 缓存 |
| `GetReleaseQueue()` | 上面那道 extract/GPU 双闸门的释放队列 |

帧槽只有 2 个：`constexpr auto kFramesInFlight = 2;`（`Engine/Source/Runtime/Source/Renderer/RHI.h`）。`FRenderManager::FrameEnd` 先记下 `ThisFrameIndex` 再前进 `CurrentFrameIndex`，所以 `WaitForFrameSlot` 里 `RHI::BeginFrame(CurrentFrameIndex)` 等的就是 `FrameEnd` 将要记录的那个 slot（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。

`RHI::BeginFrame(Slot)` 每帧做（`Engine/Source/Runtime/Source/Renderer/RHICore.cpp`）：对每个队列等到该 slot 记录的 timeline 值 → `ApplyDeferredHeapWrites` → `DrainRetireQueue` → `RetireSlot`（回收 transient command buffer）→ `TrimMemoryPages` → GPU profiler `BeginFrame` → `FShaderLibrary::FlushPendingReleases` → 重置并清空该 slot 的 command list → 交回并 flush 上一次的 staging 上传（`Upload::FlushSplit`，用 slot timeline 门控释放）→ 调整 transient ring slice、`Textures::TickPendingSwaps`。

`FRenderManager::FrameEnd` 里的顺序是：`SwapchainTarget.FinishPresent()` → `ApplyPendingResize()` → `SetMaxAnisotropy` → `GWorldManager->RenderWorlds(ThisFrameIndex)` → `SwapchainTarget.Acquire()` → 记录 present pass（`BarrierToRender`、编辑器 UI 或游戏合成 blit）→ `Present`（编辑器同步）/ `PresentAsync`（非编辑器）。编辑器还会在之后渲染拖出的二级 viewport（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。

## 6. 管线与 shader

### Shader library 与缓存

`FShaderLibrary` 以内容为 key 内部化 SPIR-V：`Commit` 对相同字节码返回同一个 handle，因此材质实例可以折叠成一个 draw；`Get(name)` 按有序 shader root 解析名字或完整虚拟路径并按需编译；`Resolve` 是唯一受检的解引用，条目被释放后返回 null；`Release` 只减强引用计数，归零后排队到下一个帧边界再释放，因为其他线程是无锁解引用 handle 的（`Engine/Source/Runtime/Source/Renderer/ShaderLibrary.h`、`Engine/Source/Runtime/Source/Renderer/ShaderLibrary.cpp`）。

```cpp
// ShaderLibrary.h
struct FShaderEntry
{
    FName            Path;
    TVector<FString> Defines;
    ERHIShaderType   Type = ERHIShaderType::None;
    TVector<uint32>  Spirv;
    uint32           ID = 0;          // process-unique, never reused
    uint64           MapHash = 0;     // key into HandlesByHash, so a release can unmap itself
    uint32           Generation = 0;  // 0 = not compiled yet; bumps on every (re)commit
    uint32           RefCount = 0;
};
```

`FShaderH` 是弱 handle，定义在 `Engine/Source/Runtime/Source/Renderer/ShaderHandle.h`。缓存（`FResolvedSurface`、`FDrawBatchKey`、`FGraphicsPipelineKey`）只持有弱 handle，条目失效时靠 `Resolve` 返回 null 得知并重新解析。

磁盘缓存是 `.lsc` 文件：`FShaderCache::kShaderCacheVersion = 2`、目录 `/Intermediates/ShaderCache`，按虚拟路径 + defines + entry point 计算 source set hash，支持普通 shader 与「生成源码」（用文本加 include 作为 key）两条路径（`Engine/Source/Runtime/Source/Renderer/ShaderCache.h`、`Engine/Source/Runtime/Source/Renderer/ShaderCache.cpp`）。打包时 `SpirvStrip::StripDebugInfo` 会去掉嵌入源码、行表和 NonSemantic debug info，只保留 driver 真正执行的指令（`Engine/Source/Runtime/Source/Renderer/SpirvStrip.h`）。

### Slang → SPIR-V

`FSpirVShaderCompiler` 实现 `IShaderCompiler`，内部用 Slang 编译到 SPIR-V：`TargetDesc.format = SLANG_SPIRV`、profile `spirv_1_6`、flags 带 `SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY | SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM`（`Engine/Source/Runtime/Source/Renderer/ShaderCompiler.cpp`）。编译以 job 运行（`ETaskPriority::High`），完成后 `FShaderCache::Save` 并调用回调 `FShaderHeader`。`Initialize()` 会 `Shaders::PrecompileNewRoots()` 批量编译 VFS 里的 shader 树；没有可编译源码（打包构建）时则从 `.lsc` 反序列化并 `FShaderLibrary::Commit`。开发构建里还会用 `spirv-val` 校验编译产物和 material template。

shader root 的顺序是引擎 → 已启用插件 → 项目的 `/Game/Shaders` → 模块注册的路径；引擎优先，插件或游戏同名文件无法遮蔽，遮蔽会被报告，必须用完整虚拟路径请求（`Engine/Source/Runtime/Source/Renderer/ShaderPaths.h`）。

shader 源码组织在 `Engine/Resources/Shaders/`：顶层是具体 pass 的 `.slang`，公共实现在 `Includes/`，材质模板在 `MaterialShader/`，粒子模板在 `Particles/`，常量头在 `Shared/SharedConstants.h`。`Shaders.Build.cs` 是 `HeaderOnly` 模块，只为让 shader 出现在 IDE 里，不参与编译；运行时自己用 Slang 编译（`Engine/Resources/Shaders/Shaders.Build.cs`）。

### FScenePipelineCache

`FScenePipelineCache` 是跨世界共享的 pipeline 缓存：以 `uint64` hash 为 key，条目同时记住所依赖的三个 `FShaderH`，一个 hash 只构建一次（`Engine/Source/Runtime/Source/Renderer/RenderManager.h`、`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。

- `Find` 查表。
- `IsPending` 表示已有构建排队或运行，调用方不必再复制 shader 代码。
- `Request` 把构建投到 worker（除非已有构建），本帧立即返回空。
- `BuildNow` 在当前线程构建，仍与其他线程的同一 hash 构建去重。
- `PurgeStale` 释放所有依赖的 shader 已经 `Resolve` 不到的 pipeline。
- `ReleaseAll` 先 `WaitForBuilds` 再释放全部。

并发上限是 CPU 核数的一半、最多 8（`MaxConcurrentPipelineBuilds`）；每完成 16 个 pipeline 且队列排空时 `RHI::SavePipelineCache()` 落盘一次（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。

热重载/失效链：材质重编译会 `FShaderLibrary::Commit` 新字节码并 `Release` 旧 handle（`Engine/Source/Runtime/Source/Assets/AssetTypes/Material/Material.cpp`）；旧 pipeline 由下一次 `FDefaultSceneRenderer::Init` 里的 `ScenePipelines().PurgeStale()` 清掉（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.cpp`）。

## 7. 场景渲染

### 真正的场景渲染器

场景渲染器不在 `Renderer/`，而在 World 侧：`IRenderScene` 是接口（`Engine/Source/Runtime/Source/World/Scene/RenderScene/RenderScene.h`），`RenderSceneFactory::Create` 默认 `MakeUnique<FDefaultSceneRenderer>`，插件可通过 `SetOverride` 替换（`Engine/Source/Runtime/Source/World/Scene/RenderScene/RenderSceneFactory.cpp`）。`FDefaultSceneRenderer` 的实现被拆到多个文件：

| 文件 | 内容 |
| --- | --- |
| `Default/DefaultSceneRenderer.cpp` | `Init`、`RenderView` 一帧编排、`RenderCaptureView` |
| `Default/SceneRenderer_Extract.cpp` | `Extract`、draw command 编译、灯光/阴影请求、cull view |
| `Default/SceneRenderer_Cull.cpp` | GPU cull dispatch、depth/cascade pyramid、skinning、meshlet cull |
| `Default/SceneRenderer_Geometry.cpp` | VisBuffer、shadow passes、decal、terrain |
| `Default/SceneRenderer_Lighting.cpp` | cluster、light cull、GBuffer、deferred lighting、透明、SSR、环境、体积云 |
| `Default/SceneRenderer_Post.cpp` | bloom、auto exposure、tonemap、DOF、TAA/velocity、SMAA |
| `Default/SceneRenderer_Overlay.cpp` | debug view、billboard、widget、sprite、text、batched line/triangle |
| `Default/SceneRenderer_Particles.cpp` | 粒子模拟/排序/渲染 |
| `Default/SceneRenderer_Editor.cpp` | picker、selection outline |
| `Default/SceneRenderer_Callbacks.cpp` | render callback 的 pipeline 与执行 |
| `Default/DefaultSceneRenderer.h` | 全部 pass 声明与 `FFrameData` |

`FDefaultSceneRenderer` 的类注释本身就是设计摘要：GPU-driven meshlet culling 到 visibility buffer，再 per-material GBuffer resolve 与 clustered lighting（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.h`）。

### 两阶段：Extract 与 RenderView

`FWorldManager::ExtractWorlds` 在主线程对每个 tick 的世界调 `World->Extract()`，并在这之前 `GetReleaseQueue().BeginExtract()`；`FWorldManager::RenderWorlds` 先对每个世界 `PrepareRender`，再对每个世界 `RenderView`，最后 `EndRender()`（`Engine/Source/Runtime/Source/World/WorldManager.cpp`）。`Extract` 把 ECS 场景快照进 `FFrameData`，`RenderView` 读同一份数据（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.h`）。

### 一帧的 pass 顺序

以下顺序直接取自 `FDefaultSceneRenderer::RenderView`（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.cpp`）。所有 pass 记录进一个 graphics command list；开启 async compute 时中间会切到 compute 队列再切回。

**Geometry 段**

1. `RmlUi::RenderWorldWidgets` → `ResetPass_Render` → `CompileDrawCommands_Render` → `PrepareSceneColorForCallbacks` + `RunRenderCallbacks(FrameStart)`。
2. `SkyCubeCapturePass` → `TexturePaintPass` → `TerrainUpdatePass` → `TerrainCullPass`。
3. `SkinningPass` → `TerrainDepthPrePass`（clear；地形优先，因为它是最便宜的大遮挡体）→ `VisBufferPass(Early)` → `CustomDepthPass`。
4. `DepthPyramidPass`（`bFreezeCulling` 时跳过）→ `MeshletCullPass(Late)` → `VisBufferPass(Late)`。
5. `GTAOPass(Prefilter)`；之后若开启 async compute，会拆到 compute 队列跑 `GTAOPass(Trace)` 与 `AerialPerspectivePass`。
6. `ClusterBuildPass`、`LightCullPass`。
7. `PointShadowPass` → `SpotShadowPass` → `CascadedShowPass` → `CascadePyramidPass`。
8. `IrradianceConvolutionPass`、`PrefilterEnvMapPass`。
9. `DecalPass` → `CloudShadowMapPass` → `EnvironmentPass`。
10. `VelocityPass`（需要时）→ `VisBufferClassifyPass` → `MaterialGBufferPass`。
11. `VolumetricCloudPass` → `FroxelInjectPass` → `FroxelIntegratePass` → `FogShaftPass`。

**Shading 段**

12. `DeferredLightingPass`；编辑器里紧跟 `PickerResolvePass`；非 Shipping 还有 `SceneDebugViewPass`。
13. `TerrainRenderPass`；非 Shipping 的 `VelocityDebugPass`；然后 `RunRenderCallbacks(AfterOpaque)`。
14. `ScreenSpaceReflectionsPass` → `AtmosphereCompositePass` → `WaterPass`。
15. `TransparentPass` → `OITResolvePass` → `UnorderedTranslucentPass`。
16. `BatchedTriangleDraw`、`BatchedLineDraw`。
17. `ParticleSimulatePass` → `ParticleSortPass` → `ParticleRenderPass`。
18. `BillboardPass`、`SpritePass`、`TextPass`；编辑器里 `WidgetPickerPass` 与 `IssuePickerReadback`；`RunRenderCallbacks(AfterTranslucency)`。
19. `UnderwaterPass` → `DepthOfFieldPass` → `BloomPass` → `AutoExposurePass` → `ToneMappingPass` → `PostProcessMaterialPass`；`RunRenderCallbacks(AfterPostProcess)`。
20. SMAA 三段（`GetSMAAMode() != Off` 时）→ `TemporalResolvePass`（TAA 开启时）→ 编辑器 `SelectionOutlinePass`。
21. `RunRenderCallbacks(Overlay)` → `WidgetPass`；非 Shipping 的 `DebugTextPass`。
22. 每个 capture view：`PointAtView` + `SetSceneRoot` + `RenderCaptureView`；然后 `ReflectionProbeBakePass`。
23. `RmlUi::RenderWorldUI` → `Barriers::RasterToRead` → `CollectStreamingFeedback` → `SnapshotMotionState` → `RHI::Submit`。

### GBuffer、光照与阴影的位置

这不是传统 deferred GBuffer：`VisBufferPass` 用 mesh shader 写入 visibility buffer（存 VisID + 1，0 表示空），`VisBufferClassifyPass` 与 `MaterialGBufferPass` 再 per-material resolve，最后 `DeferredLightingPass` 做光照（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/SceneRenderer_Geometry.cpp`、`SceneRenderer_Lighting.cpp`）。

阴影分三类 pass：`PointShadowPass`、`SpotShadowPass`、`CascadedShowPass`，共享 shadow atlas；`AllocateShadowTiles` 等在 `SceneRenderer_Extract.cpp`，`BindShadowBatchPipeline`/`DrawShadowBatch` 在 `SceneRenderer_Geometry.cpp`。

transparent 有两个路径：排序的 `TransparentPass`、`OITResolvePass`（Order-Independent Transparency），以及 `UnorderedTranslucentPass`（`SceneRenderer_Lighting.cpp`）。

### mesh shader 与 meshlet 管线

mesh pipeline 用 `RHI::CreateMeshShaderPipeline(Task, Mesh, Fragment, Desc, Consts)` 创建；`FDefaultSceneRenderer::MakeGraphicsBuild` 里当 key 带 `MS` 时走它，并将 task shader 传空——因为 `MeshletCull.slang` 在任何 draw 记录前就已经 compact 过：

```cpp
// DefaultSceneRenderer.cpp
// Task-less by construction, since MeshletCull.slang compacted before any draw was recorded.
const RHI::FPipelineH Pipeline = MS.IsSet()
    ? RHI::CreateMeshShaderPipeline(RHI::FShaderSource{}, MS.Source(), PS.Source(), Desc, Consts)
    : RHI::CreateGraphicsPipeline(VS.Source(), PS.Source(), Desc, Consts);
```

`VisBufferPass` 的 mesh / pixel shader 来自 `FShaderLibrary`，例如 `VisBufferPixel.slang` 与 `Batch.VisBufferMeshShader`；`MeshletCullPass` 从 `MeshletCull.slang` 取 cull shader，`DispatchMeshletBlockBuild` 从 `BuildMeshletBlocks.slang` 取 block builder（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/SceneRenderer_Geometry.cpp`、`SceneRenderer_Cull.cpp`；shader 文件在 `Engine/Resources/Shaders/MeshletCull.slang`、`Engine/Resources/Shaders/BuildMeshletBlocks.slang`）。

### 与编辑器的交界

`Engine/Editor/Source` 里与场景渲染相关的部分主要有：`Thumbnails/ThumbnailScene.cpp` 直接建一个世界、`World->Extract()` 后调 `Scene->RenderView(FrameIndex)` 并读 display texture；`Tools/Screenshot/ScreenshotCapture.cpp` 把 render scene 的 HDR/display 图像 copy 到 CPURead 分配做 readback。它们都依赖 `IRenderScene` 接口，不依赖具体后端（`Engine/Editor/Source/Thumbnails/ThumbnailScene.cpp`、`Engine/Editor/Source/Tools/Screenshot/ScreenshotCapture.cpp`）。

## 8. 材质

`FMaterialManager` 拥有 GPU 上的材质 uniform 表，并给每个 `CMaterialInterface` 分配一个 slot；slot 复用优先走 FreeList，表增长按高水位倍增（`Engine/Source/Runtime/Source/Renderer/MaterialManager.h`、`Engine/Source/Runtime/Source/Renderer/MaterialManager.cpp`）。

关键点：

- `FMaterialUniforms` 是 POD，`Vectors[MAX_VECTORS]`、`Scalars[MAX_SCALARS]`、`Textures[MAX_TEXTURES]`、`Flags`、`OpacityClipValue`、`CollectionIndices`；`static_assert` 保证 16 字节对齐（`Engine/Source/Runtime/Source/Renderer/MaterialTypes.h`）。
- 单参数修改走 `UpdateMaterialUniformRange`，只上传子区间，而不是整块 592 字节。
- CPU 端 `Mirror` 就是 shader 读到的字节，因此 `CopySlotTextureIDs` 直接从中取纹理 ID，不会与实际采样不一致。
- 表增长是 staged 的：新 buffer 的镜像 copy 真正落地前不会发布，避免读到回收中的显存；`GetMaterialBuffer`/`GetMaterialSpan` 是发布点。
- `FMaterialCollectionManager` 是另一张固定大小的参数 collection 表，slot 0 保留为零。
- `RemoveMaterialSlot` 不带 owner 引用，正是给延迟释放路径用的：调用时 owner 已经销毁。

材质图编译在编辑器侧：`CompileMaterialGraph` 把图编译成材质的 shader stage（PS/VS，PBR 域还有 MS/VisBuffer/Deferred），提交进 shader library，填充 `Material->Textures/Parameters` 并 `PostLoad`；它由材质编辑器与场景导入器共享，另有非阻塞的 `BeginMaterialGraphCompile`/`FinishMaterialGraphCompile`（`Engine/Editor/Source/UI/Tools/NodeGraph/Material/MaterialGraphCompile.h`）。GPU 侧的材质 flag 与 shader 常量必须一致：`EMaterialGPUFlags` 的注释写明 “Must match EMaterialFlags in Common.slang”（`Engine/Source/Runtime/Source/Renderer/MaterialTypes.h`）。

## 9. 几何与网格

网格数据模型以 meshlet 为中心，`MeshData.h` 里每种结构都与 GPU mirror 对齐（`Engine/Source/Runtime/Source/Renderer/MeshData.h`）：

| 结构 | 大小约束 |
| --- | --- |
| `FMeshlet` | 32 B，32 字节必须匹配 `Common.slang` 的 GPU mirror |
| `FMeshletSphere` | 16 B，cull 每 meshlet 必读 |
| `FMeshletCone` | 4 B，SNORM8 backface cone，编码时向上取整以保证保守 |
| `FMeshletHeaderGPU` | 144 B，包含所有数组的地址与 `MeshletCount` 等权威边界 |
| `FMeshletBonePalette` | 8 B |

`FMeshletData` 是 CPU 侧的 meshlet 集合（meshlets、两种顶点、三角形、球、cone、骨骼 palette）；`FMeshResource::FMeshBuffers` 把多个 stream 放进**一个** `GeometryBlock` 分配，用偏移而不是多个 `VkBuffer`（`Engine/Source/Runtime/Source/Renderer/MeshData.h`）。

```cpp
// MeshData.h: FMeshletHeaderGPU carries the authoritative bounds with the pointers.
uint32 MeshletCount;

// Entries in BonePalettes, and the bound every reader of it uses. Zero on a static mesh...
uint32 BonePaletteCount;
```

顶点格式（`Engine/Source/Runtime/Source/Renderer/Vertex.h`）：`FSourceVertex` 32 B、`FSourceSkinnedVertex` 44 B 是 CPU-only 的源格式；GPU 格式 `FMeshletVertex` 28 B、`FMeshletSkinnedVertex` 36 B。`FMeshletVertex` 的位置是相对 meshlet anchor 的 16-bit 偏移，解码在 `MeshQuantization.h`。

meshlet 量化：每个 meshlet 用 16-bit 每轴位置，配合 24-bit signed anchor 与共享 exponent；`ComputeMeshletQuantization` 用 `meshopt_computePositionExponent` 选指数，`EncodeMeshletPosition` 写入偏移（`Engine/Source/Runtime/Source/Renderer/MeshQuantization.h`）。

meshlet header 的存储是 slot 寻址的 slab，而不是裸地址：`MeshletHeaderSlab` 保证 slot 永远不悬空，`kNullSlot = 0` 永远是一个“没有几何”的零 header，释放的 slot 按 FIFO 回收，最坏情况读到另一个 mesh 的合法 header 而不是踩内存（`Engine/Source/Runtime/Source/Renderer/MeshletHeaderSlab.h`）。

## 10. 纹理流式加载

纹理默认只带“内联尾部”加载：长边 ≤ `kInlineMipMaxDimension = 256` 的 mip 常驻，更大的 mip 按需提升（`Engine/Source/Runtime/Source/Renderer/TextureData.h`）。`FTextureStreamingManager` 每帧执行 Wanted → Budgeted → Residency：`WantedFirstMip` 只看屏幕覆盖质量，`BudgetedFirstMip` 在预算内逐个 mip 地全局均匀降级，`Residency` 再双向跟随预算；`Pin`/`Unpin` 是带引用计数的硬承诺，pin 住的纹理不参与预算驱逐（`Engine/Source/Runtime/Source/Renderer/TextureStreamingManager.h`）。

流式反馈来自 GPU：`SubmitMaterialFeedback` 的入参按 **material slot** 索引，`Masks[Slot]` 是每个像素对该材质所需分辨率的 OR；per-texture 需求由 uniform mirror 的纹理 ID 展开得到（`Engine/Source/Runtime/Source/Renderer/TextureStreamingManager.h`、`Engine/Source/Runtime/Source/Renderer/MaterialManager.h`）。

residency 变化之所以可行，是因为 `RHI::Textures::Recreate` 保留并重指向已存在的 bindless slot，而不是重新分配。纹理的 `ResourceID` 被烘进每个采样它的材质 uniform block 且永不回访，所以如果 slot 发生移动，材质会静默指向别的纹理。Staged swap 的固定顺序是 `Recreate → 上传所有 mip → CommitRecreate`，中间不能跨帧边界（`Engine/Source/Runtime/Source/Renderer/RHITexture.h`）。

上传路径是 `RHIUpload`：`UploadBuffer` / `UploadTexture` 把拷贝排进一个 batch，`BeginFrame` 里 `Upload::FlushSplit` 在 graphics 与 transfer 队列间拆分并提交；batch 完成由 timeline 判定。释放资源前必须 `CancelTexture` / `CancelBuffer`，否则排队的拷贝会打在已销毁的 `VkImage` 或已释放的地址上（`Engine/Source/Runtime/Source/Renderer/RHIUpload.h`）。

## 11. 后处理与调试绘制

后处理 pass 都在 `Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/SceneRenderer_Post.cpp`：`BloomPass`、`AutoExposurePass`、`ToneMappingPass`、`DepthOfFieldPass`、`PostProcessMaterialPass`、`VelocityPass`、`TemporalResolvePass`，以及 SMAA 三段 `SMAAEdgeDetectionPass` / `SMAABlendWeightPass` / `SMAANeighborhoodBlendPass`。SMAA 的查找表 `AreaTex.h` / `SearchTex.h` 位于 `Engine/Source/Runtime/Source/Renderer/SMAA/`，由 `FDefaultSceneRenderer::InitSharedResources` 上传成 `Shared.SMAAArea` / `Shared.SMAASearch`（`Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.cpp`）。

调试绘制有两个入口：

- `IPrimitiveDrawInterface`（`Engine/Source/Runtime/Source/Renderer/PrimitiveDrawInterface.h`）提供 `DrawLine`/`DrawBox`/`DrawSphere`/`DrawCapsule`/`DrawCone`/`DrawFrustum` 等高层原语，`IRenderScene` 继承它。
- `FImmediateLineRenderer`（`Engine/Source/Runtime/Source/Renderer/ImmediateLineRenderer.h`）是底层实现：两个 channel（`DepthTested`、`XRay`），每个工作线程用原子预留一段顶点（8 KiB / 256 条线），`BeginFrame`/`Snapshot`/`CloseFrame` 跨 `RHI::kFramesInFlight + 2` 个 slot 管理，容量按上一帧实际需求增长。

scene 侧的 overlay pass 在 `SceneRenderer_Overlay.cpp`：`SceneDebugViewPass`、`BillboardPass`、`WidgetPickerPass`、`WidgetPass`、`SpritePass`、`TextPass`、`DebugTextPass`、`BatchedLineDraw`、`BatchedTriangleDraw`。编辑器 picker 与 selection outline 在 `SceneRenderer_Editor.cpp`。

## 12. 性能分析与 GPU 计时

`GpuProfiling` 是构建期特性：`Engine/Build/BuildConfiguration.json` 里默认 `auto`，`Engine/Build/LuminaFeatures.BuildRules.cs` 解析为 `bNonShipping`（Debug + Development 开，Shipping 关），并定义宏 `LUMINA_WITH_GPU_PROFILING`。

```csharp
// LuminaFeatures.BuildRules.cs
GpuProfiling => bNonShipping,
...
if (IsActive(Target, GpuProfiling))
{
    Definitions.Add("LUMINA_WITH_GPU_PROFILING");
}
```

开启后 `FGPUProfiler`（`Engine/Source/Runtime/Source/Renderer/GPUProfiler.h`）用每个 frame slot 一个 timestamp query pool 计时，支持的最大 scope 数 256、历史 240 帧；`BeginFrame` 先解析上一帧再武装本帧。它由现有的 `RHI::CmdBeginMarker` / `CmdEndMarker` 驱动——也就是说每个已命名的 pass marker 自动成为一个计时 scope，再加 `TracyVkContext` 的 GPU zone：

```cpp
// VulkanRHI.cpp: CmdBeginMarker
#if defined(LUMINA_WITH_GPU_PROFILING)
    FGPUProfiler::Get().BeginScope(CL, Name);
#endif

#if defined(TRACY_ENABLE)
    tracy::VkCtx* ZoneContext = GTracyGPUContexts[(uint32)List.Queue];
    ...
#endif
```

运行期开关是 console variable `r.GPUProfiler.Enabled`（默认 0），`IsEnabled()` 还要求 `RHI::SupportsTimestamps()`。GPU timestamp 的 RHI 接口（`SupportsTimestamps`、`CreateTimestampPool`、`CmdWriteTimestamp`、`ReadTimestamps`）全部包在 `#if defined(LUMINA_WITH_GPU_PROFILING)` 内。

Tracy 本身是另一个特性：`Tracy` → `LUMINA_WITH_TRACY` / `TRACY_ENABLE`，`RmlUiProfiling` 需要它（`Engine/Build/LuminaFeatures.BuildRules.cs`）。每帧一个 Tracy Vulkan context，按队列命名。

带边界的 GPU span 由 `TGPUSpan<T>` 表达：16 字节内含地址与元素数，`static_assert(sizeof(TGPUSpan<uint32>) == 16)`，镜像 shader 侧的 `GPUSpan<T>`；所有构造函数都从 allocation 推导 count 并做截断，避免“数量和地址不同源”导致的越界读（`Engine/Source/Runtime/Source/Renderer/GPUSpan.h`）。

## 13. 错误处理与崩溃调试

`ICrashTracker` 是抽象（`Engine/Source/Runtime/Source/Renderer/ErrorHandling/CrashTracker.h`），Vulkan 实现 `FVulkanCrashTracker`（`Engine/Source/Runtime/Source/Renderer/ErrorHandling/Vulkan/VulkanCrashTracker.h`）。

- **validation**：`DeviceDesc.bValidation` 决定是否启用 `VK_LAYER_KHRONOS_validation`；`CreateInstance` 里通过 `VkLayerSettingsCreateInfoEXT` 配置 `validate_sync`、`syncval_message_extra_properties` 以及 `gpuav_*` 各项；GPU-AV 的开关是 `gpuav_enable`，仅在 `DeviceDesc.bValidation` 为真时启用。`--validate=<check>` / `--novalidate=<check>` 可在运行期逐个覆盖检查项，其中 `--validate=instrument` 会打开逐 shader 的 GPU-AV instrumentation（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）。默认值来自 `Validation` 特性：`LUMINA_WITH_VALIDATION` 时 `bValidation = true`（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。
- **Aftermath**：`Aftermath` 特性定义 `WITH_AFTERMATH`，`FVulkanCrashTracker::GetDeviceCreatePNext` 提供 Aftermath 的 `pNext`，崩溃回调走 `GpuCrashDumpCallback` / `OnShaderDebugInfo`；注册的 shader 有 256 MiB 预算，超出后按注册顺序淘汰最旧的（`Engine/Source/Runtime/Source/Renderer/ErrorHandling/Vulkan/VulkanCrashTracker.h`、`VulkanCrashTracker.cpp`）。
- **Radeon GPU Detective**：`RadeonGpuDetective` 特性定义 `WITH_RGD`，只在 AMD 设备上打印 `.rgd` 抓取指引；它不链接 SDK，靠 driver 写 dump（`Engine/Source/Runtime/Source/Renderer/ErrorHandling/Vulkan/VulkanCrashTracker.cpp`、`Engine/Build/LuminaFeatures.BuildRules.cs`）。
- **device lost / breadcrumbs**：`VK_CHECK` 在失败且结果是 `VK_ERROR_DEVICE_LOST` 时调用 `RHI::HandleDeviceLost`，否则弹对话框并 `LUMINA_DEBUG_BREAK`；`FGpuBreadcrumbs` 是一张 GPU 可写的 marker 表（2048 个、名字 64 字符、最大深度 32），device lost 时给出仍未结束的 marker 报告（`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanMacros.h`、`Engine/Source/Runtime/Source/Renderer/ErrorHandling/Vulkan/VulkanBreadcrumbs.h`）。
- **RenderDoc**：`FRenderDoc` 封装 in-application API，`IsAttached()`、`TriggerCapture()`、`EndFrameCapture()` 等；`CreateDevice` 在 `bufferDeviceAddressCaptureReplay` 可用且 RenderDoc 已附加时打开该 feature，让 capture 可重放（`Engine/Source/Runtime/Source/Renderer/RenderDocImpl.h`、`Engine/Source/Runtime/Source/Renderer/API/Vulkan/VulkanRHI.cpp`）。编辑器菜单里有 “RenderDoc Capture” 项（`Engine/Editor/Source/UI/EditorUI.cpp`）。

## 14. 线程模型

Lumina 的渲染**没有独立的渲染线程**：帧循环、场景 extract、命令录制与提交都在主线程上，并行只来自 task system 的 job。

- 主循环在 `FApplication::Run`：先 `RenderManager->WaitForFrameSlot()`，再 `ProcessMessages`，再 `GEngine->Update()`（`Engine/Source/Runtime/Source/Core/Application/Application.cpp`）。注释解释了顺序原因：等待是输入延迟窗口里最长的部分，所以在采样鼠标之前做。
- `FEngine::Update` 的 `FrameEnd` 阶段依次做 `GWorldManager->ExtractWorlds()`、`BeginImmediateLines`、`Render().FrameEnd()`（`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）。`FrameEnd` 里 `RenderWorlds` 才真正录制。
- `FWorldManager::RenderWorlds` 在 `r.ParallelWorldRender`（默认 `true`）且存活世界多于一个时，用 `Task::ParallelFor` 并行录制多个世界；只有一个世界时走串行路径（`Engine/Source/Runtime/Source/World/WorldManager.cpp`）。
- 提交是串行化的：`RHI::Submit` 取 `GCore.SubmitMutex`，后端每个队列还有自己的 `FMutex`（`QueueLockFor`）；`RHIInternal.h` 之外唯一合法的提交入口是 `RHI::Submit`。插件若需要直接提交，用 `RHI::Native::FScopedSubmitLock` 与引擎序列化（`Engine/Source/Runtime/Source/Renderer/RHICore.cpp`、`Engine/Source/Runtime/Source/Renderer/RHINative.h`）。
- pipeline 编译在 background job 上执行（`FScenePipelineCache::Pump` 用 `Jobs::RunJob(..., EJobPriority::Background, ...)`），避免 assist-wait 的线程继承 driver 编译（`Engine/Source/Runtime/Source/Renderer/RenderManager.cpp`）。
- 资源生命周期规则：GPU 资源一律 `RHI::Retire`，在对应 slot 的 `BeginFrame` 里等 timeline 越过后销毁；材质 slot 与纹理这种“描述 GPU 资源、必须刚好在资源死亡瞬间停止描述”的 CPU 状态走 `FRenderReleaseQueue`/`RetireCallback`；shader 用弱 `FShaderH`，悬空的可由 `Resolve` 判定；meshlet header 用永不悬空的 slot（`Engine/Source/Runtime/Source/Renderer/RHICore.cpp`、`Engine/Source/Runtime/Source/Renderer/RenderRelease.h`、`Engine/Source/Runtime/Source/Renderer/ShaderHandle.h`、`Engine/Source/Runtime/Source/Renderer/MeshletHeaderSlab.h`）。

## 15. 相关文档

- [`../README.md`](../README.md) — 引擎总览、构建特性说明、硬件要求。
- [`EngineOverview.md`](EngineOverview.md) — 仓库布局、模块与目标、启动流程、主循环与更新阶段。
- [`ECSGuide.md`](ECSGuide.md) — ECS、`FRegistry`、组件存储与 typed view（World 侧数据的来源）。
- [`ReflectionSystem.md`](ReflectionSystem.md) — 材质属性等如何被反射驱动并进入编辑器 UI。
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — 编码规范与类型命名约定。
- [`../DEPENDENCIES.md`](../DEPENDENCIES.md) — Slang、RenderDoc、Tracy 等外部依赖的版本与来源。
- [`../Engine/Tools/LuminaBuildTool/README.md`](../Engine/Tools/LuminaBuildTool/README.md) — 模块/Target 规则、构建特性如何落到预处理器定义。

---

### 已知差异与局限

本节记录文档与代码之间、或文档与 README 之间**尚未对齐**的地方，供后续核实与修正。以下每一条都已用仓库内的 `grep` 复核。

- **同步是显式的**：全仓库没有 RenderGraph 或同类自动推导实现。实际同步是调用方显式写出的 `RHI::CmdBarrier` 与 `RHI::Barriers::*` helper。README 的「自动资源追踪与 barrier 插入」应按此理解，不要预期存在一个会自动插入 barrier 的层。
- **`--gpuvalidation` 暂无运行期实现**：`Engine/Build/LuminaFeatures.BuildRules.cs` 会在该特性开启时定义 `LUMINA_WITH_GPU_VALIDATION`，但全仓库（含 `Engine/Resources/Shaders/`）没有该宏的任何消费者，也找不到解析 `--gpuvalidation` / `--nogpuvalidation` 的代码。当前 GPU-AV 实际由 `RHI::FDeviceDesc::bValidation`（即 `Validation` 特性）控制。`README.md` 与 `Engine/Applications/RHITests/Source/Main.cpp` 的用法文本都提到了这个开关，属于待补齐的部分。
- **`EBackend::Metal` / `DX12`**：仅在枚举中作为 `@TODO` 存在，没有实现。

