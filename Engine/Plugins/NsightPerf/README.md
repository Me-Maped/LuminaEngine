# NsightPerf

仅编辑器可用的插件，它将 **NVIDIA Nsight Perf SDK** 集成为 Lumina 编辑器内的实时 GPU 指标 HUD
（周期性采样器）。

## 它的作用

- 新增一个 **Tools → Plugins → Nsight Perf HUD** 窗口：一块持续更新的 GPU
  指标看板（SM 占用率、吞吐量、显存带宽等），数据来自 NvPerf 的 *periodic
  sampler*，并使用引擎的 ImPlot 渲染。
- 仅在窗口打开时进行采样。

## 如何启用/禁用

插件**本身**就是可选择启用的单位。它位于 `Engine/Plugins/NsightPerf/`，始终参与构建，
每个项目在自己的 `.lproject` 中启用/禁用它：

```json
"Plugins": [ { "Name": "NsightPerf", "Enabled": false } ]
```

它是 `EnabledByDefault: true`。没有全局构建开关。

## 为什么它在 Core 阶段加载

NvPerf 的 periodic sampler 要求特定的 Vulkan **instance 和 device 扩展在
设备创建时即已启用**。引擎在启动期间创建设备，早于大多数插件的加载，因此本
插件的模块在 **Core** 加载阶段（早于 `RHI::CreateDevice`）加载，并通过
`RHI::Native::RegisterDeviceCreationRequest` 注册所需扩展（见
`Engine/Source/Runtime/Renderer/RHINative.h`）。它还通过
`RHI::Native::GetNativeDeviceHandles()` 取得实时设备句柄，该函数返回的是*不透明*句柄
（不会有任何 Vulkan 类型泄漏出后端），由本插件自行将其重新解释为 `Vk*`。

## 要求

- 一块**支持 Nsight Perf 的 NVIDIA GPU** 及其驱动。在其他 GPU 上窗口仍会打开，
  并说明该功能不可用。
- `Engine/Source/ThirdParty/NsightPerf/` 下内置的 SDK（头文件 + `nvperf_grfx_host`
  的 lib/DLL）。该 DLL 会在构建时被复制到可执行文件旁边。

## 线程

periodic sampler 与引擎的图形队列共用同一个队列，但引擎在渲染线程上提交帧，
而本工具逐帧的工作运行在游戏线程上。两个线程对同一个队列并发执行 `vkQueueSubmit`
会导致设备丢失。因此本工具把所有会提交的 NvPerf 调用
（初始化、逐帧的 `OnFrameEnd`、销毁）都包在 `RHI::Native::FScopedSubmitLock` 中，
它获取的正是 RHI 用于自身提交的那把 mutex，从而将它们串行化、绝不竞争。
ImGui 渲染仍留在锁之外。

代价：当该工具打开时，在 `OnFrameEnd` 前后持有这把锁可能会在每帧短暂阻塞渲染
线程（这是仅存在于性能分析场景的卡顿）。一个开销更低的设计是通过逐帧的 RHI hook
把 sampler 调度到渲染线程上；出于简洁考虑选择了 submit-lock 方案。

## NVIDIA 权限

在未向非管理员开放 GPU 性能计数器之前，`BeginSession` 会以
`NVPA_STATUS_INSUFFICIENT_PRIVILEGE` 失败：NVIDIA Control Panel → Developer →
“Allow access to the GPU performance counters to all users”（或以管理员身份运行
编辑器）。该工具会报告这一状态，而不是直接失败。
