# Lumina ECS 指南

本文档介绍 Lumina Engine 的 Entity Component System（ECS）：实体句柄、`FRegistry`、组件存储、typed view、生命周期 signal、事件与命令队列，以及如何定义一个新 component。

> 本文的 API 签名、宏名与语义均取自仓库中真实存在的源码，关键结论后附对应源文件路径，示例代码来自真实头文件或引擎/测试源码。

## 1. 概览

Lumina 的 ECS 是一套自研的 sparse-set 实现。README 在架构一节的定位是「自研的稀疏集（sparse-set）Entity Component System，支持按类型划分的存储布局、类型化视图和生命周期信号」（`README.md`）。源码注释里能读到的设计目标：

- `FRegistry`：「One world's entities and component pools. Storages are keyed by dense type id, so lookup is an index.」（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）
- `FSparseSet`：「One component pool, entity mapping and payload together. No virtuals, so nothing here dispatches.」（`Engine/Source/Runtime/Source/World/ECS/SparseSet.h`）
- `FEntity`：打包进 32 位的 generational handle，「packed into 32 bits so it stays blittable for script and shaders」（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）
- `FComponentTypeInfo`：类型擦除侧需要的全部信息「resolved once at registration rather than through a meta call」（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）

与 OOP 的取舍遵循 `CONTRIBUTING.md` 的面向数据设计：数据与行为分离、普通 struct 存数据、批量处理、热路径避免虚调用、组合优于继承（`CONTRIBUTING.md`）。ECS 只负责存储与生命周期；每帧的更新顺序与并行批次由 `CWorld` 与 system scheduler 负责（见 `Engine/Source/Runtime/Source/World/World.h` 中 `CWorld::TickSystems` 的调度注释）。

一个重要的边界：`FRegistry` 不是玩法逻辑的公开 API——「Gameplay, tooling, and C# go through CWorld's typed component/entity/singleton wrappers instead -- the registry is not public API.」（`Engine/Source/Runtime/Source/World/World.h`）

## 2. 核心概念

| 概念 | 作用 | 主要头文件 |
| --- | --- | --- |
| Entity（`FEntity`） | 20 位 index + 12 位 version 的 generational 句柄，4 字节、可平凡复制 | `Engine/Source/Runtime/Source/World/ECS/Entity.h` |
| Registry（`FRegistry`） | 一个 world 的实体表与全部组件池；负责 create/destroy、storage 查找、view 构造 | `Engine/Source/Runtime/Source/World/ECS/Registry.h` |
| Component | 被注册为 component 的普通 struct；空 struct 是 tag，带字段的是 data component | `Engine/Source/Runtime/Source/World/ECS/ComponentType.h` |
| Storage（`FSparseSet`） | 一个 component type 的 sparse/dense 池：实体到 dense slot 的映射 + payload | `Engine/Source/Runtime/Source/World/ECS/SparseSet.h` |
| Typed Storage（`TComponentStorage<T>`） | 只持有 `FSparseSet*` 的 typed lens，按编译期已知偏移访问 | `Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h` |
| View（`TView`） | 多组件交集/排除的 typed 迭代器，按最小池选择 driver | `Engine/Source/Runtime/Source/World/ECS/View.h` |
| Context（`FRegistryContext`） | 每 registry 一份的 singleton 存储，按同一套 dense type id 索引 | `Engine/Source/Runtime/Source/World/ECS/Context.h` |

支撑设施：`TEntityMap<T>`（entity-index 分页索引的「每实体一个值」表，`Engine/Source/Runtime/Source/World/ECS/EntityMap.h`）、`FComponentSignal`/`FComponentSignals`（`Engine/Source/Runtime/Source/World/ECS/Signal.h`）、`FEventDispatcher`（`Engine/Source/Runtime/Source/World/ECS/EventDispatcher.h`）、`FCommandBus`（`Engine/Source/Runtime/Source/World/ECS/CommandBus.h`）、`FECSObjectReferenceProvider`（`Engine/Source/Runtime/Source/World/ECS/ECSReferenceProvider.h`）。

## 3. 实体（Entity）

### 3.1 句柄结构与打包

`FEntity` 把 index 与 version 打包进一个 `uint32`，参数写死：

```cpp
struct FEntity
{
    static constexpr uint32 IndexBits   = 20;
    static constexpr uint32 VersionBits = 32 - IndexBits;
    static constexpr uint32 IndexMask   = (1u << IndexBits) - 1u;
    static constexpr uint32 VersionMask = (1u << VersionBits) - 1u;
    // One slot index is spent on the null handle, so this is the highest addressable slot.
    static constexpr uint32 MaxIndex = IndexMask - 1u;
    // Reserved for retired dense slots, so no live handle ever carries it.
    static constexpr uint32 TombstoneVersion = VersionMask;
    uint32 Value = Constants::kIndexNoneU32;
};
```

（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）

打包公式是 `Value = (Index & IndexMask) | ((Version & VersionMask) << IndexBits)`；`GetIndex()` / `GetVersion()` / `GetPacked()` 取回三部分。头文件末尾用 `static_assert` 固定了「4 字节」「trivially copyable」「slot 0 版本 0 是真实 entity」等约束（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）。

### 3.2 哨兵值

- `Constants::kIndexNoneU32` 的真实定义是 `~0u`（`Engine/Source/Runtime/Source/Lumina.h`）。
- `NullEntity = FEntity::FromPacked(Constants::kIndexNoneU32)`；`IsNull()` 判断 `GetIndex() == IndexMask`——null handle 占用最高 index 值，所以 `MaxIndex = IndexMask - 1u`（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）。
- `MakeTombstone(NextFreeDenseIndex)` 生成的 handle 只有 version 是 `TombstoneVersion`，index 段用来串联 dense 池里的 free list（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）。

### 3.3 generation 与悬空检测

`Destroy(Entity)` 会先 fire `EntityDestroyed`，把实体从所有 storage 摘除，然后执行 `Record = FEntity(FreeEntityHead, Record.GetNextVersion())`：被销毁的 slot 进入 free list 并携带下一个 version（`Engine/Source/Runtime/Source/World/ECS/Registry.cpp`）。`GetNextVersion()` 会跳过保留的 `TombstoneVersion`：「Skips the reserved tombstone version so a recycled slot stays addressable.」（`Engine/Source/Runtime/Source/World/ECS/Entity.h`）

于是旧句柄在复用的 slot 上一定 version 不匹配，`IsValid()` / `Contains()` 都返回 false。测试中的真实断言：`Check(C.GetIndex() == A.GetIndex(), "a freed slot is reused"); Check(C.GetVersion() != A.GetVersion(), "a reused slot carries a new version");`（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。有效性判断本身是 `EntityRecords[Index] == Entity`（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。

### 3.4 `TEntityMap<T>` 的槽位管理（`EntityMap.h`）

`TEntityMap<T>` 是「每实体一个值」的表，注释说明其动机是「looked up through paged entity-index slots like a component pool instead of by hashing」（`Engine/Source/Runtime/Source/World/ECS/EntityMap.h`）。

- `PageSize = 4096`；`Find` 先按 `Index / PageSize` 找稀疏页，再以「slot 的 version == 查询 version」判定命中，陈旧句柄自然 miss（`Engine/Source/Runtime/Source/World/ECS/EntityMap.h`）。
- `FindOrAdd` 缺失时默认构造；`Remove` 是 swap-remove，会移动最后一个值并让指向它的指针失效（`Engine/Source/Runtime/Source/World/ECS/EntityMap.h`）。
- 真实用例：Box3D 用 `ECS::TEntityMap<FBodyRecord> RigidBodies;` 存每实体的物理 body 记录（`Engine/Source/Runtime/Source/Physics/API/Box3D/Box3DPhysicsScene.h`）；行为测试见 `Engine/Source/Runtime/Tests/EntityMapTests.cpp`（`StaleVersionMisses` 验证 version 不匹配 miss，`RemoveKeepsMovedEntryReachable` 验证 swap-remove 后其余条目仍可达）。

## 4. Registry

`FRegistry` 可以被 move，且「Moving it hands the source a fresh state, so a moved-from registry can still create entities.」（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）它持有 `EntityRecords`、allocation state、全部 storage、context 与两个实体级 signal（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。

### 4.1 实体生命周期 API

以下签名来自 `Engine/Source/Runtime/Source/World/ECS/Registry.h`，语义转述自同文件与 `Registry.cpp` 的注释：

| API | 语义与适用场景 |
| --- | --- |
| `FEntity Create()` | 优先复用 free list 的 slot，否则追加新 slot（version 0）。 |
| `FEntity Create(FEntity Hint)` | 在槽位空闲时物化该确切 handle，「which is what loading a saved world needs」；若 `Hint` 指向仍存活的 slot，返回一个全新的 `Create()` handle 而不碰撞。 |
| `FEntity CreateInFreshSlot()` | 「Never recycles a freed slot, so an editor undo that restores a deleted handle still finds it free.」 |
| `void Destroy(FEntity Entity)` | 无效句柄直接返回；否则先 broadcast `OnEntityDestroyed`，从所有 storage 摘除（每个 storage 先 fire `OnDestroy`），再让 slot 带新 version 进 free list。 |
| `FEntity ReserveEntity()` / `void MaterializeReserved(FEntity)` / `void ReleaseReserved(FEntity)` | 预留、物化、归还一个 handle。「Thread-safe.」预留后 `IsValid` 为 false，直到 `MaterializeReserved`「makes a reserved handle live and fires the created signal, exactly as Create would have」；`ReleaseReserved`「returns a reservation that will never be materialized to the free list」。 |
| `bool IsValid(FEntity Entity) const` | 检查 `EntityRecords[Index] == Entity`。 |
| `size_t NumEntities() const` | 返回 `LiveEntityCount`。 |
| `void Clear()` | 「Destroys every entity and empties every pool. The pools themselves survive.」 |
| `void Reserve(size_t EntityCount)` | 预留 `EntityRecords` 容量。 |
| `void Swap(FRegistry& Other) noexcept` | 「Trades whole worlds, which is what loading into a pending registry and going live needs.」 |
| `void Compact()` | 「Drops tombstones from every stable pool, which is worth doing after a bulk delete.」 |

真实使用（`Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.cpp`）：

```cpp
ECS::FEntity EntityID = Registry.Create();
Registry.Emplace<STransformComponent>(EntityID).SetWorldTransform(Transform);
Registry.Emplace<SNameComponent>(EntityID, EntityName);
Registry.EmplaceOrReplace<FNeedsTransformUpdate>(EntityID);
return EntityID;
```

### 4.2 CWorld / World 与 FRegistry 的关系

- `CWorld` 内部持有 `ECS::FRegistry RegistryPending;` 与 `EntityRegistry;`，以及 `ECS::FEventDispatcher SingletonDispatcher;`、在注册之后初始化的 `ECS::FCommandBus CommandBus;`（`Engine/Source/Runtime/Source/World/World.h`）。`RegistryPending` + `Swap` 正是「先加载到 pending registry，再整体切成 live」的用法（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。
- `GetEntityRegistry()` 被刻意设为 `private`：「Raw registry handle. Intentionally private -- gameplay (C++/C#) and tooling use the typed wrappers above; engine internals reach it through friendship (FSystemContext, FWorldManager, ...).」（`Engine/Source/Runtime/Source/World/World.h`）
- 引擎内部需要整 registry 操作时走自由函数：`RUNTIME_API ECS::FRegistry& GetWorldRegistry(CWorld& World);`，实现是 `return World.GetEntityRegistry();`（`Engine/Source/Runtime/Source/World/World.h`，`Engine/Source/Runtime/Source/World/World.cpp`）。
- 玩法侧的 typed facade 都在 `CWorld` 上：`CreateEntity()`、`ConstructEntity(FName, const FTransform&)`、`DestroyEntity(FEntity)`、`IsValidEntity(FEntity) const`、`GetNumEntities() const`、`ClearAllEntities()`、`EmplaceComponent<T>()`、`GetComponent<T>()`、`TryGetComponent<T>()`、`View<T...>()` 等（`Engine/Source/Runtime/Source/World/World.h`）。

## 5. 组件存储

### 5.1 组件类型 id 从哪来

- `using FComponentTypeID = uint16;`，`InvalidComponentTypeID = 0xFFFFu`，上限 `MaxComponentTypes = 2048`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。
- `GetComponentTypeID<T>()` 是每模块一份的 per-type 缓存（`static std::atomic<FComponentTypeID> Cached`），首次调用向 `FComponentTypeRegistry::Get().Acquire(MakeComponentTypeInfo<T>())` 申请；`Acquire` 按名字幂等，因此 DLL、插件重载、多线程竞争都会落到同一个 id（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`，`Engine/Source/Runtime/Source/World/ECS/Registry.cpp`）。
- 类型名来自编译器自己的签名：「Extracted from the compiler's own signature, so an unreflected component still has a stable name.」（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。测试验证未反射类型的名字就是限定名 `ECSBench::FPosition`（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。
- 已被反射的类型在 `FComponentTypeRegistry::BindStruct` 里绑定它的 `CStruct`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`，`Engine/Source/Runtime/Source/World/ECS/Registry.cpp`）。

### 5.2 Component 概念与 tag

```cpp
template<typename T>
concept CComponent = std::is_object_v<T> && !std::is_pointer_v<T> && !std::is_const_v<T>
	&& !std::is_volatile_v<T> && !std::is_array_v<T>;

template<typename T>
concept CTagComponent = CComponent<T> && TComponentTraits<T>::bEmpty;

template<typename T>
concept CDataComponent = CComponent<T> && !TComponentTraits<T>::bEmpty;
```

（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）

空 struct 是 tag（`bEmpty = std::is_empty_v<T>`）。tag 没有 payload，所以 `Get` / `TryGet` / `Patch` 对 tag 是编译错误，`Emplace` 对 tag 返回 `void`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`，`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`）。`SelfCheck.cpp` 用 concept 把这些约束逐条 `static_assert`（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。

### 5.3 布局 trait

组件通过声明同名成员覆盖默认 trait，默认值在 `FComponentTraitDefaults`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）：

| trait | 默认值 | 说明 |
| --- | --- | --- |
| `static constexpr bool InPlaceDelete` | `false` | 删除时留 tombstone 而非 swap-and-pop，保持元素地址不变 |
| `static constexpr EComponentLayout Layout` | `EComponentLayout::Automatic` | `Automatic` / `Packed` / `Paged` |
| `static constexpr uint32 PageSize` | `1024` | 必须是 2 的幂，池用 shift + mask 索引 |

`TComponentTraits<T>::bPaged = !bEmpty && (InPlaceDelete || Layout == Paged || Layout == Automatic)`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。也就是说**当前代码里 data component 默认（`Automatic`）就是 paged**，为了指针稳定；`SelfCheck.cpp` 明确断言 `static_assert(TComponentTraits<FPlain>::bPaged, "an automatic component is paged for pointer stability");`（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。要强制 packed 必须显式写 `Layout = EComponentLayout::Packed`；`InPlaceDelete && Packed` 会触发 `static_assert`（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。

真实覆盖示例（`Engine/Applications/ECSBench/Source/BenchCommon.h`）：

```cpp
struct FStablePosition
{
	static constexpr bool InPlaceDelete = true;
	float X = 0.0f, Y = 0.0f, Z = 0.0f;
};
```

`BenchLayoutSweep.cpp` 与 `SelfCheck.cpp` 里还有 `Layout = EComponentLayout::Packed` / `Layout = EComponentLayout::Paged` / `PageSize = 64` 的真实写法（`Engine/Applications/ECSBench/Source/BenchLayoutSweep.cpp`，`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。

### 5.4 `FSparseSet` 的稀疏/密集布局

一个池由三部分构成（`Engine/Source/Runtime/Source/World/ECS/SparseSet.h`）：

- **sparse**：`TVector<FEntity*> SparsePages`，每 page `SparsePageSize = 4096` 个 `FEntity` slot；按需分配，「which keeps a sparse world from paying for a flat array」。
- **dense**：`TVector<FEntity> Dense`，紧凑实体数组，foreach 就是遍历它。
- **payload**：`uint8* PackedData`（packed）或 `TVector<uint8*> PayloadPages`（paged，page 大小来自 `PageSize`）；tag 的 `ElementSize == 0`，不分配 payload。

sparse slot 存 `FEntity(DenseIndex, EntityVersion)`；`Contains` 先拒绝 null 再比较 version：`return !Entity.IsNull() && FindSlot(Entity.GetIndex()).GetVersion() == Entity.GetVersion();`（`Engine/Source/Runtime/Source/World/ECS/SparseSet.h`）。

删除有两条路径（`Engine/Source/Runtime/Source/World/ECS/SparseSet.cpp`）：**swap-and-pop**（默认）把最后一个元素移到被删的 dense slot 再 `pop_back()`；**in-place delete**（`bInPlaceDelete`）把 dense slot 变 `MakeTombstone(FreeDenseHead)`，free list 串在 dense 数组里，靠 `Compact()` 在批量删除后压掉 tombstone。遍历统一**从后往前**，这样「removing the current entity only pulls an already visited one into its slot」（`Engine/Source/Runtime/Source/World/ECS/SparseSet.h`）。`SelfCheck.cpp` 用 300 个实体逐实体验证了 packed / paged / tag / exclude / runtime view 等各种遍历在「边走边删」时不重不漏（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。

复杂度（来自 `Engine/Source/Runtime/Source/World/ECS/SparseSet.cpp` 实现）：

| 操作 | 复杂度 |
| --- | --- |
| `Contains` / `GetDenseIndex` / `FindSlot` | O(1)：一次 sparse page 索引 + version 比较 |
| `Emplace` fast path | O(1)：`Dense.push_back` + 写 sparse slot |
| `Emplace` slow path | 可能分配 sparse page；packed 按 2 倍扩容并搬运 payload，paged 只分配新 page |
| `RemoveEntity`（swap-and-pop） | O(1) + 移动一个元素 |
| `RemoveEntity`（in-place delete） | O(1)，留 tombstone |
| 单池 `ForEach` | O(dense size)，跳过 tombstone |
| `Compact` | O(dense size) |

### 5.5 `TComponentStorage<T>` 与 Registry 入口

`TComponentStorage<T>` 「Holds a pointer and nothing else, so it costs the same to pass as one.」（`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`），并有 `static_assert(sizeof(TComponentStorage<FEntity>) == sizeof(void*))`。它提供 `Contains` / `Num` / `IsEmpty` / `HasTombstones` / `GetDenseSize` / `GetDenseData` / `Get` / `TryGet` / `Emplace` / `EmplaceOrReplace` / `GetOrEmplace` / `RemoveEntity` / `Reserve` / `Each` / `ForEachDense` / `ForEachDenseWhere`；`GetAtDense` 在 `sizeof(T)` 已知时直接算地址，「compiles to the same address math a hand-written array would」（`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`）。

`FRegistry` 上的 typed 入口（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）：

| API | 说明 |
| --- | --- |
| `Emplace<T>(Entity, Args...)` | 构造并加入；data component 返回 `T&`，tag 返回 `void`；fire `OnConstruct` |
| `EmplaceOrReplace<T>(...)` | 已存在则替换并 fire `OnUpdate`，否则 fire `OnConstruct` |
| `GetOrEmplace<T>(...)` | 已存在则原样返回，否则构造并 fire `OnConstruct` |
| `Get<T>(Entity)` / `TryGet<T>(Entity)` | 只接受 `CDataComponent`；`Get` 用 `FindStorage`，读取不会创建池 |
| `Patch<T>(Entity, Mutators...)` | 依次应用 mutator，然后 fire `OnUpdate` |
| `Remove<T>(Entity)` | 先 fire `OnDestroy`，再 `RemoveEntity`；不存在返回 false |
| `HasAll<Ts...>` / `HasAny<Ts...>` | 多类型成员测试，要求类型不重复 |
| `ReserveComponents<T>(Count)` | 预分配一个池，避免增长时重定位 |
| `GetStorage<T>()` / `FindStorage<T>()` | 前者会 assure（可能创建池），后者只查 |
| `NamedStorage<T>(FName)` / `FindNamedStorage<T>(FName)` | 同类型下的第二个独立池，「which is how one tag becomes many」 |

`Get` 之所以用 `FindStorage`：「a read never creates a pool and never needs a call.」（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）

### 5.6 Context（singleton）

`FRegistryContext` 是每 registry 一份的 singleton 存储，按同一套 dense type id 索引（`Engine/Source/Runtime/Source/World/ECS/Context.h`）。入口是 `FRegistry::Ctx()` 以及 `EmplaceSingleton<T>` / `GetSingleton<T>` / `TryGetSingleton<T>` / `HasSingleton<T>` / `EraseSingleton<T>`（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。`Emplace` 覆盖已存在的 singleton，`GetOrEmplace<T>()` 保留已有值（`Engine/Source/Runtime/Source/World/ECS/Context.h`）。`CWorld` 自己用它存 `FTweenManager` / `FTimerManager`：`return EntityRegistry.Ctx().Get<FTweenManager>();`（`Engine/Source/Runtime/Source/World/World.h`）。

## 6. Views 与查询

### 6.1 构造与 driver 选择

`FRegistry::View` 有两个重载（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）：`template<CComponent... TInclude> NODISCARD TView<TExclude<>, TInclude...> View();` 与 `template<CComponent... TInclude, CComponent... TExcl> NODISCARD TView<TExclude<TExcl...>, TInclude...> View(TExclude<TExcl...>);`。

`TView` 的语义（`Engine/Source/Runtime/Source/World/ECS/View.h`）：include 是**交集**；exclude 用 `FMembershipProbe` 过滤（「The probes are loop-invariant, so a scan pays for the page arrays once rather than per element.」）；driver 取扫描成本最小的 include 池（`ScanCost` 按 dense slot 数计，空池优先），若任一 include 池不存在则 view 直接为空；include 只有单一池时走 `bIsDirectScan` 快路径，不再逐元素测 include 成员。`TExclude<Ts...>` 只是 filter tag，`View<A, B>(TExclude<C>{})` 是唯一写法。

真实写法（`Engine/Source/Runtime/Source/World/Entity/Systems/CameraRigSystem.cpp`）：

```cpp
auto FollowView = Registry.View<SCameraFollowComponent, STransformComponent>(ECS::TExclude<SDisabledTag, SSpringArmComponent>{});
```

### 6.2 View API

| API | 说明 |
| --- | --- |
| `ForEach(TFunc Func)` | 回调收到 `FEntity` 与每个 include data component 的引用；tag 只过滤，不进参数 |
| `ForEachEntity(TFunc Func)` | 只遍历实体，component 自己用 `Get` 取 |
| `ForEachInRange(size_t Begin, size_t End, TFunc Func)` | 按 dense slot 区间切片，供并行任务用 |
| `Each()` / `begin()` / `end()` | `Each()` 是 tuple 迭代（`for (auto [Entity, Value] : View.Each())`）；range-for 只遍历实体 |
| `Get<T>(Entity)` / `TryGet<T>(Entity)` / `GetAll(Entity)` | 读取 view 内的 include data component；`GetAll` 一次性返回全部引用 tuple |
| `Contains(Entity)` | 同时测试 include 与 exclude |
| `NumCandidates()` / `Num()` | driver 的 `Num()`，结果数的**上界**而非精确值 |
| `NumDenseSlots()` | driver 的 `GetDenseSize()`，并行切分必须用它（tombstone 也算 slot） |
| `GetDriver()` | 实际驱动扫描的 `FSparseSet*` |

签名与注释来自 `Engine/Source/Runtime/Source/World/ECS/View.h`：`Num()` 是上界因为「The driver is the smallest included pool, so the real count is at most this.」；`NumDenseSlots()` 特别提醒「Dense slots, not live entities, because splitting on the live count drops a tombstoned tail.」

真实使用（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）：

```cpp
Registry.View<FPosition, FVelocity>().ForEach([&](ECS::FEntity, FPosition&, FVelocity&) { ++Both; });
```

`SelfCheck.cpp` 还验证了 `View.NumDenseSlots() > View.NumCandidates()`，以及把整段 dense slot 切成 32 大小的 chunk 后 `ForEachInRange` 与整段 `ForEach` 结果完全一致（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。

### 6.3 类型擦除与 System 用法

编译期不知道组件类型时用 `FRuntimeView`：`AddInclude(const FSparseSet&)`、`AddExclude(const FSparseSet&)`、`GetDriver()`、`Num()`、`Contains(Entity)`、`ForEach(TFunc)`，支持 range-for；可由 `Registry.FindStorage(ECS::GetComponentTypeID<T>())` 的池拼出（`Engine/Source/Runtime/Source/World/ECS/View.h`，`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。

`FSystemContext` 提供 `CreateView<Ts...>(Args...)`、`ForEach<Ts...>(Function, Args...)` 与 `ParallelForEach<Ts...>(...)`；后者「Fans an already built view out over the workers, running it inline when it is too small to be worth splitting.」，切分用 `View.NumDenseSlots()`（`Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h`）。`SKinematicsSystem` 就是这么用的：`Context.CreateView<SVelocityComponent>()`（`Engine/Source/Runtime/Source/World/Entity/Systems/KinematicsSystem.cpp`）。`ForEachInRange` 在 range 内从后往前走，因此各 worker 负责一段、互不重叠（`Engine/Source/Runtime/Source/World/ECS/View.h`）。

## 7. 生命周期信号与事件

### 7.1 `FComponentSignals`：即时组件生命周期 signal

每个 storage 自带 `FComponentSignals`，含三个 `FComponentSignal` 通道 `OnConstruct` / `OnDestroy` / `OnUpdate`；监听器是 typed free function + payload：`struct FComponentListener { using FCallback = void (*)(void* Payload, FRegistry& Registry, FEntity Entity); FCallback Callback = nullptr; void* Payload = nullptr; };`（`Engine/Source/Runtime/Source/World/ECS/Signal.h`）。

两种绑定方式：

```cpp
// 成员函数，按标识绑定/解绑
EntityRegistry.GetSignals<STransformComponent>().OnConstruct.Connect<&ThisClass::OnTransformComponentConstruct>(this);

// 自由函数 + payload（CommandBusTests.cpp 中的真实用法）
Registry.OnEntityCreated().Connect(&CountCreated, &Created);
```

（`Engine/Source/Runtime/Source/World/World.cpp`，`Engine/Source/Runtime/Tests/CommandBusTests.cpp`）

`Broadcast` 是同步的：`if (Listeners.empty()) return;` 之后直接调 `BroadcastSlow`，没有排队（`Engine/Source/Runtime/Source/World/ECS/Signal.h`）。`FSignalConnection` 是类型擦除的订阅凭证，`Release()` 会断开并清空 `Signal`（`Engine/Source/Runtime/Source/World/ECS/Signal.h`）。

各 API 触发 signal 的时机（来自 `Engine/Source/Runtime/Source/World/ECS/Registry.h` 与 `Registry.cpp`）：

| 操作 | signal |
| --- | --- |
| `Emplace<T>` / `GetOrEmplace<T>`（新增） | `OnConstruct` |
| `EmplaceOrReplace<T>`（新增 / 已存在） | `OnConstruct` / `OnUpdate` |
| `Patch<T>` | 写完 mutator 后 `OnUpdate` |
| `Remove<T>` | 先 `OnDestroy`，再移除 |
| `Destroy(Entity)` | 先 `EntityDestroyed`，再对每个拥有的 component 依次 `OnDestroy` 并移除 |
| `Create` / `MaterializeReserved` | `EntityCreated` |

`CWorld` 初始化时用这套 signal 做实体级联，例如「Also per entity, which fires before any storage drops its component, so OnDetach can still reach the sibling components it bound to.」——脚本 `OnDetach` 绑在 `OnEntityDestroyed()` 而非组件 `OnDestroy` 上（`Engine/Source/Runtime/Source/World/World.cpp`）。`Registry.GetSignals<T>()` 会 `AssureStorage<T>()`，因此监听的类型即使还没有池也会被创建（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。

### 7.2 `FEventDispatcher`：world 级 typed 事件

`FEventDispatcher` 以同一套 dense type id 为下标持有各事件类型的 `TEventSink<TEvent>`（`Engine/Source/Runtime/Source/World/ECS/EventDispatcher.h`）：`Connect<&Handler>(Instance)` / `Connect<&Handler>()` 与对应 `Disconnect`；`Broadcast(const TEvent&)` 会先复制 listener 列表再派发，「Copied first, so a handler that binds or unbinds during dispatch cannot invalidate the walk.」`Sink<TEvent>()` 按需创建 sink，`Trigger<TEvent>(Event)` / `Trigger<TEvent>()` 立即广播；未被订阅的事件类型 `Trigger` 直接返回（`Engine/Source/Runtime/Source/World/ECS/EventDispatcher.h`）。

真实用例：`SystemContext.EventSink<FSwitchActiveCameraEvent>().Connect<&ThisClass::OnChangeCameraEvent>(this);` 与 `SingletonDispatcher.Trigger<FSwitchActiveCameraEvent>(FSwitchActiveCameraEvent{Entity});`（`Engine/Source/Runtime/Source/World/World.cpp`）。`FSystemContext::DispatchEvent<T>()` 在并行 system 下会延迟：「A parallel system's event waits for the batch to end, since listeners are free to change the world.」——它查 `ECS::FCommandBus::ShouldDefer()`，为 true 时把 `Trigger` 排进 command bus（`Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h`）。

### 7.3 `FCommandBus`：排队延后的结构变更

「Each thread records into its own slot without a lock, and a flush applies creates, then commands in record order, then destroys.」（`Engine/Source/Runtime/Source/World/ECS/CommandBus.h`）

| API | 说明 |
| --- | --- |
| `FEntity Create()` | 向 registry `ReserveEntity()`，handle 记进当前线程 `Creates`，立即可作为命令 target |
| `void Destroy(FEntity Entity)` | 记进 `Destroys` |
| `Emplace<T>(Entity, Args...)` | 「Adds the component, or replaces it when the entity already has one.」实际调用 `EmplaceOrReplace` |
| `Remove<T>(Entity)` / `Patch<T>(Entity, Mutator)` | 延迟移除 / 延迟 patch |
| `Enqueue(TFunc&& Func)` | 自定义命令；`Func` 接受 `FRegistry&` 或不接受参数 |
| `uint32 Flush()` | 主线程专用；每个 slot 依次 creates → commands → destroys |
| `void Discard()` | 「Drops every pending command without running it, returning reserved handles to the registry.」 |
| `static bool ShouldDefer()` / `FDeferScope(bool)` | 「True on a worker, or on a thread the scheduler marked because it runs a parallel system.」/ 设置线程 deferring 标志的 RAII |

（全部来自 `Engine/Source/Runtime/Source/World/ECS/CommandBus.h`）

`Flush()` 的顺序在 `Engine/Source/Runtime/Source/World/ECS/CommandBus.cpp`：每轮先对所有 slot `MaterializeReserved`，再跑 commands，最后跑 destroys；「Commands recorded while applying run in the same flush.」但最多 64 轮，防止命令自反馈成死循环。

真实用法（`Engine/Source/Runtime/Tests/CommandBusTests.cpp`）：

```cpp
ECS::FRegistry Registry;
ECS::FCommandBus Bus(Registry);

const ECS::FEntity Entity = Bus.Create();
Bus.Emplace<FBusValue>(Entity, FBusValue{ 7 });
EXPECT_FALSE(Registry.IsValid(Entity));

Bus.Flush();
ASSERT_TRUE(Registry.IsValid(Entity));
```

`CWorld::TickSystems` 会在每个 stage 前、以及每个并行 batch 结束后调用 `FlushCommands()`；运行 batched system 时用 `const ECS::FCommandBus::FDeferScope Defer(!S.System->GetAccess().bExclusive);` 把「声明了 access 的 system 的结构变更」推迟到 batch 结束（`Engine/Source/Runtime/Source/World/World.cpp`）。

### 7.4 三者对比

| 机制 | 时效 | 典型用途 |
| --- | --- | --- |
| `FComponentSignal` | 即时，emplace/remove/patch 当场 broadcast | 组件构造/销毁时的资源绑定、派生状态失效 |
| `FEventDispatcher::Trigger` | 即时（`DispatchEvent` 在并行 system 下改为延迟） | world 级事件，如切换 active camera |
| `FCommandBus` | 排到 queue，直到下一次 sync point `Flush()` | 并行 system 里的 create/destroy/add/remove/patch |

## 8. 定义一个新组件

以仓库中真实存在的 `SLifetimeComponent` 为例（逐行照抄 `Engine/Source/Runtime/Source/World/Entity/Components/LifetimeComponent.h`）：

```cpp
#pragma once
#include "Core/Object/ObjectMacros.h"
#include "LifetimeComponent.generated.h"

namespace Lumina
{
	REFLECT(Component, HideInComponentList)
	struct RUNTIME_API SLifetimeComponent
	{
		GENERATED_BODY()
		
		/** Time in seconds before the entity is automatically destroyed (0 = never). */
		PROPERTY()
		float Lifetime = 0.0f;
	};
}
```

步骤与真实宏：

1. **声明 struct + `GENERATED_BODY()`**。`GENERATED_BODY(...)` 在非反射解析阶段展开成 `<FileID>_<Line>_GENERATED_BODY` 宏名；`REFLECT` / `PROPERTY` / `FUNCTION` 本身在非反射解析阶段是空宏，由 Reflector 生成代码（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`）。
2. **用 `REFLECT(Component, ...)` 标记 ECS component**。Reflector 的 specifier 表是权威定义：`Component` 是 `Runtime` consumer，含义为「Marks the struct as an ECS component. Registers component meta.」（`Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h`）。
3. **包含生成的 `.generated.h`**，例如 `#include "LifetimeComponent.generated.h"`（`Engine/Source/Runtime/Source/World/Entity/Components/LifetimeComponent.h`）。
4. **注册是自动的**：带 `Component` specifier 的 struct，其生成的 `Construct_CStruct_*` 会调用 `::Lumina::Meta::RegisterComponentMeta<T>();`（`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedStruct.cpp`）。`RegisterComponentMeta<T>` 做两件事：`RegisterComponentOps(T::StaticStruct(), &GetComponentOps<T>())` 与（非 shipping 下）注册 access validator（`Engine/Source/Runtime/Source/World/Entity/Components/Component.h`）。`RegisterComponentOps` 会调用 `ECS::FComponentTypeRegistry::Get().BindStruct(Ops->TypeId, Struct)`（`Engine/Source/Runtime/Source/World/Entity/EntityUtils.cpp`）。
5. **给字段加 `PROPERTY(...)`**，决定它是否进入反射/序列化/编辑器。`PROPERTY(NoSerialize)` 会「Excludes the property from serialization.」（`Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h`）
6. **在编辑器中出现**：`REFLECT(Component, Category = "Camera")` 会在组件选择器里分组显示，`HideInComponentList` 则「Hides the component from the scene editor's add-component list.」（`Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h`）真实对照：`SCameraComponent` 用 `REFLECT(Component, Category = "Camera")` 加多个 `PROPERTY(Editable, Category = "Camera", ...)` 暴露在检视器里（`Engine/Source/Runtime/Source/World/Entity/Components/CameraComponent.h`）；`SLifetimeComponent` / `STagComponent` 用 `HideInComponentList` 主动藏起来（`Engine/Source/Runtime/Source/World/Entity/Components/LifetimeComponent.h`，`Engine/Source/Runtime/Source/World/Entity/Components/TagComponent.h`）。

**只给 C++ 用的组件**不需要 `Component` specifier。真实例子是 `SVelocityComponent`，只有 `REFLECT()`（`Engine/Source/Runtime/Source/World/Entity/Components/VelocityComponent.h`），由 `SKinematicsSystem` 通过模板 API 读写（`Engine/Source/Runtime/Source/World/Entity/Systems/KinematicsSystem.cpp`）。代价是它不在 `ForEachComponentStruct` 列表里，也不会被反射驱动的序列化处理——序列化侧的真实日志是 `"[ECS] Entity {}: '{}' is reflected but not a component; skipping ({} bytes)."`（`Engine/Source/Runtime/Source/World/Entity/EntityUtils.cpp`）。

覆盖布局 trait 只需在 struct 里声明同名 `static constexpr` 成员，真实写法见 `Engine/Applications/ECSBench/Source/BenchCommon.h` 的 `struct FFatStable { static constexpr bool InPlaceDelete = true; float M[16] = {}; };`。

使用新组件：

```cpp
const ECS::FEntity Entity = Registry.Create();
Registry.Emplace<SLifetimeComponent>(Entity, SLifetimeComponent{ .Lifetime = 3.0f });
SLifetimeComponent* Lifetime = Registry.TryGet<SLifetimeComponent>(Entity);
```

`Emplace` 的返回类型是 `TComponentStorage<T>::FEmplaceResult`，对 data component 是 `T&`，对 tag 是 `void`（`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`，`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。

## 9. 与反射和序列化的关系

- 每个被注册的 component type 都有一张 `FComponentOps` 表，把 `Get` / `Has` / `Emplace` / `Remove` / `Patch` / `StaticStruct` / `EmplaceSerialized` 等解析成普通函数指针，使类型擦除路径不带 vtable（`Engine/Source/Runtime/Source/World/Entity/Components/Component.h`）。
- 反射驱动的序列化按 `CStruct` 的反射名字找类型，再调 `Ops->EmplaceSerialized(Registry, Entity, Ar)`；它先把数据读进临时对象再 `EmplaceOrReplace`，「Read into a temporary first, so a corrupt archive cannot leave a live component half-written.」（`Engine/Source/Runtime/Source/World/Entity/Components/Component.h`，`Engine/Source/Runtime/Source/World/Entity/EntityUtils.cpp`）。
- 写侧走 `Entry.Struct->SerializeTaggedProperties(Ar, Entry.Set->GetRaw(Entity));`，所以字段是否被写由该字段的 `PROPERTY` 标记决定（`Engine/Source/Runtime/Source/World/Entity/EntityUtils.cpp`）。批量保存用 `Registry.ForEachEntityExcept<FEditorComponent>(...)` 跳过 editor-only 实体（`Engine/Source/Runtime/Source/World/Entity/EntityUtils.cpp`）。
- 引用追踪（GC / object reference 重定向）由 `FECSObjectReferenceProvider` 负责：遍历每个 world 的 registry，用 `CStruct` 反射信息 walk 每个活着的 component；没绑定 `CStruct` 的 storage 会被跳过，注释是「an unreflected storage holds no properties to walk」（`Engine/Source/Runtime/Source/World/ECS/ECSReferenceProvider.cpp`）。

更完整的反射、`PROPERTY` / `FUNCTION` specifier 与序列化细节见 [`ReflectionSystem.md`](ReflectionSystem.md)。

## 10. 与 C# 脚本的互操作

C# 侧通过 `EntityRegistry` 访问 ECS，它是「the C# mirror of ECS::FRegistry / FEntityRegistry」，且「Returned wrappers point at the live component, so writes persist.」（`Engine/Source/LuminaSharp/World/EntityRegistry.cs`）

| C# API | 对应 C++ |
| --- | --- |
| `Valid(Entity)` | `FRegistry::IsValid` |
| `TryGet<T>(Entity)` / `Get<T>(Entity)` | `FRegistry::TryGet<T>` / `Get<T>` |
| `Has<T>(Entity)` | `FRegistry::HasAll<T>` |
| `Emplace<T>(Entity)` / `Add<T>(Entity)` / `GetOrAdd<T>(Entity)` | `GetOrEmplace` 语义（幂等，不覆盖已有值） |
| `Remove<T>(Entity)` / `Patch<T>(Entity)` | `FRegistry::Remove<T>` / `Patch<T>`（「mutate via Get first, then Patch」） |
| `OnConstruct<T>` / `OnDestroy<T>` / `OnUpdate<T>` | `FComponentSignals` 三通道，返回可 `Dispose` 的 `RegistrySubscription` |
| `View<T1>(Exclude Filter = default)` … `View<T1,T2,T3,T4>` | `FRegistry::View<Ts...>(TExclude<...>{})` |

（`Engine/Source/LuminaSharp/World/EntityRegistry.cs`，`Engine/Source/LuminaSharp/World/View.cs`）

脚本类继承 `CEntityScript`，通过 `public EntityRegistry Registry => World.Registry;` 拿到 registry（`Engine/Source/LuminaSharp/Scripting/EntityScript.cs`，`Engine/Source/Runtime/Source/Scripting/EntityScript.h`）。exclude 用 `Exclude.Of<T1>()` / `Exclude.Of<T1, T2>()` / `Exclude.Of<T1, T2, T3>()` 构造，最多三个（`Engine/Source/LuminaSharp/World/View.cs`）；C# 侧只支持 1–4 个 include（`Engine/Source/LuminaSharp/World/EntityRegistry.cs`），这是与 C++ 变参 view 的一个明确差异。

生成绑定的细节、`[Property]` 与热重载规则见 [`ReflectionSystem.md`](ReflectionSystem.md)；项目侧的脚本与目录组织见 [`../Templates/Blank/README.md`](../Templates/Blank/README.md)。

## 11. 性能注意事项

**数据布局**

- data component 默认（`Automatic`）是 paged，换来「a held reference survives an emplace into the same pool mid-iteration」（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）；需要连续大块、随机访问密集时显式选 `Layout = EComponentLayout::Packed`。
- `Packed` 按 2 倍扩容，且「Realloc first, because rpmalloc can often widen the block in place and skip the copy entirely.」；非平凡可移动类型走 `TypeInfo.Relocate`（`Engine/Source/Runtime/Source/World/ECS/SparseSet.cpp`）。
- `InPlaceDelete` 用 tombstone 换地址稳定，代价是遍历跳洞与周期性 `Compact()`。
- 预分配用 `FRegistry::Reserve(size_t)` 和 `ReserveComponents<T>(Count)`，前者「removes the relocation a growing packed pool would otherwise pay」（`Engine/Source/Runtime/Source/World/ECS/Registry.h`）。

**迭代与查询**

- 所有池遍历从后往前，所以「swap-and-pop only ever pulls an already visited element into the slot being removed」；`SelfCheck.cpp` 在各种遍历方式下逐实体验证了「边走边删」不重不漏（`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`，`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。
- 多组件 view 由最小的池驱动（`PickDriver` / `ScanCost`），尽量让 view 里有一个成员数远小于其它池的 component 当 driver；`BenchGroupHeadroom.cpp` 专门测「scanning the large pool instead at 1 in 64」的代价（`Engine/Source/Runtime/Source/World/ECS/View.h`，`Engine/Applications/ECSBench/Source/BenchGroupHeadroom.cpp`）。
- exclude 的成本来自每元素一次 membership probe；`BenchExcludeCost.cpp` 分别测空 tag 池与 1/1000、1/100、1/10、1/2 的 exclude 相对 plain 的开销（`Engine/Applications/ECSBench/Source/BenchExcludeCost.cpp`）。
- **迭代中不要向同一个池添加元素**：`CheckWalkDidNotReallocate` 会在 debug 下断言「A component was added to a pool while iterating it, which reallocated the arrays the walk was reading」（`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`）。
- 并行遍历必须按 `NumDenseSlots()` 切片（tombstone 也算 dense slot）；`FSystemContext::ParallelForEach` / `ParallelForEachView` 是官方入口（`Engine/Source/Runtime/Source/World/ECS/View.h`，`Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h`）。

**热路径**

- `FSparseSet` 没有虚函数；`TComponentStorage<T>` 只持有一个指针，并有 `static_assert` 保证「costs the same to pass as one」（`Engine/Source/Runtime/Source/World/ECS/SparseSet.h`，`Engine/Source/Runtime/Source/World/ECS/ComponentStorage.h`）。
- 组件池查找是 dense type id 的数组索引而非类型名 hash；`GetComponentTypeID<T>()` 每条读路径只做一次 relaxed atomic load，「unlike a lazy static this carries no thread-safe-init guard on every read」（`Engine/Source/Runtime/Source/World/ECS/Registry.h`，`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。
- 类型擦除路径走函数指针表；`FComponentTypeInfo` 里也有 `DefaultConstruct` / `CopyConstruct` / `Destruct` / `Relocate` 的平面函数指针（`Engine/Source/Runtime/Source/World/ECS/ComponentType.h`）。
- 需要「每实体一个值」的辅助表时优先 `TEntityMap<T>` 这类 index-keyed 结构而非 hash map；`BenchEntityMap.cpp` 对比的正是「build/lookup, hash map」与「build/lookup, dense index array」（`Engine/Source/Runtime/Source/World/ECS/EntityMap.h`，`Engine/Applications/ECSBench/Source/BenchEntityMap.cpp`）。
- 这些规则对应 `CONTRIBUTING.md` 的面向数据设计：数据与行为分离、批量处理、热路径减少虚调用、组合优于继承（`CONTRIBUTING.md`）。

**基准测试**

`Engine/Applications/ECSBench/` 是官方 ECS 基准。默认 200000 个实体、5 次取最好（`Engine/Applications/ECSBench/Source/ECSBench.cpp`），运行前先跑 `SelfCheck`（`Engine/Applications/ECSBench/Source/SelfCheck.cpp`）。它覆盖：`BenchLayoutSweep.cpp`（8/16/24/32/48/64/128 字节下 `Packed` 对 `Paged` 的 emplace / iterate / random Get）、`BenchExcludeCost.cpp`（单组件 view 上 tag exclude 的成本曲线）、`BenchEntityMap.cpp`（hash map 对 entity-index dense 数组的 build / lookup）、`BenchGroupHeadroom.cpp`（两组件 join 在匹配/不匹配不同比例下的 view 成本，以及 group 的边界移动与 emplace+remove 成本）。这些 case 的结果是运行时打印的 ns/op，源码中不固化任何数字（`Engine/Applications/ECSBench/Source/BenchLayoutSweep.cpp`，`Engine/Applications/ECSBench/Source/BenchCommon.h`）。

## 12. 相关文档

- [`EngineOverview.md`](EngineOverview.md) —— 引擎整体架构与模块划分。
- [`RenderingArchitecture.md`](RenderingArchitecture.md) —— 渲染管线；`CWorld` 如何把 ECS 数据 extract 到 render scene。
- [`ReflectionSystem.md`](ReflectionSystem.md) —— `REFLECT` / `PROPERTY` / `FUNCTION`、反射元数据、序列化与 C++ ⇄ C# 绑定。
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) —— 命名约定、`TObjectHandle` 规则、面向数据设计与代码风格。
