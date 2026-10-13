# Lumina Engine 反射系统

本文描述 Lumina Engine 的反射系统：源码标记宏、Reflector（libclang 代码生成器）、生成产物、运行时元数据模型，以及自动序列化、编辑器属性 UI、C# 绑定这三条消费链。所有结论来自仓库中实际存在的源码；路径均相对于仓库根目录。

反射不是三个彼此独立的系统，而是**一份元数据被三处消费**：运行时序列化按它读写字段，编辑器按它生成 details panel，Reflector 按它为 C# 生成 thunk 与 wrapper。`README.md` 的 Features 把它概括为「反射系统驱动自动序列化与编辑器集成」，`Engine/Source/Runtime/Source/ModuleAPI.h` 则说明了 C# 一侧为什么必须走按名字解析的导出。

## 1. 为什么需要反射

| 消费者 | 真实入口 | 读取的元数据 |
| --- | --- | --- |
| 自动序列化 | `CObject::Serialize(FArchive&)` 调用 `Class->SerializeTaggedProperties(Ar, this)`（`Engine/Source/Runtime/Source/Core/Object/Object.cpp`） | `CStruct::AllProperties`、每个 `FProperty` 的 `Name` / `TypeFlags` / `Flags`、`ShouldSerialize()` |
| 编辑器属性 UI | `FPropertyTable::RebuildTree()` 遍历 `Struct->GetProperties()`（`Engine/Editor/Source/UI/Properties/PropertyTable.cpp`） | `FProperty::IsVisible()`、`GetPropertyDisplayName()`、`GetMetadata(...)` |
| C# 绑定 | Reflector 生成的 `LuminaSharp_*` thunk，运行期由 `NativeBindings.Resolve(Module, EntryPoint)` 解析（`Engine/Source/LuminaSharp/Interop/NativeBindings.cs`） | `CStruct` / `FProperty` 的类型名、offset、可脚本化标记 |

三者共享同一份运行时模型：`CField` / `CEnum` / `CStruct` / `CClass` 描述类型，`FProperty` 与 `FFunction` 描述成员（`Engine/Source/Runtime/Source/Core/Object/Class.h`、`Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.h`）。

## 2. 整体数据流

1. 头文件里用 `REFLECT` / `GENERATED_BODY` / `PROPERTY` / `FUNCTION` 标记类型与成员，并把 `<Stem>.generated.h` 作为**最后一个** include；宏本体在 `Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`。
2. LuminaBuildTool 收集所有 `bEnableReflection == true` 的模块，写出输入文档 `Reflection_Files.json`（`Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs`）。该文件位于 `<Target.IntermediateDirectory>/Reflection_Files.json`，路径由 `ReflectionStep.CreateActions` 拼出。
3. 构建工具把每个模块的生成目录设为 `Intermediates/Reflection/<Platform>/<Target>/<Type>-<Config>/<Module>`（`Engine/Tools/LuminaBuildTool/Graph/TargetAssembler.cs`），并把 8 个 unity shard 直接加入 `Module.GeneratedSourceFiles`。
4. Reflector 进程读取 JSON，用 libclang 解析头文件（`Engine/Applications/Reflector/Source/Reflector.cpp`、`Engine/Applications/Reflector/Source/Reflector/Clang/ClangParser.cpp`）。
5. 解析结果落入 `FReflectionDatabase`，再经校验（`generated.h` 是否最后包含、被 `PROPERTY` 标记的类型是否已反射、头文件是否循环包含）。
6. `FCodeGenerator::GenerateCode()` 为每个头文件写出 `<File>.generated.h` 与 `<File>.generated.cpp`，并在该头文件有 C# 可暴露类型或 `SCRIPT_EXPORT` 自由函数时写出 `<File>.generated.cs`，同时写出 `ReflectionUnity_<N>.gen.cpp`（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`）。
7. 生成源文件随模块一起编译；其中静态注册对象在翻译单元初始化时把构造函数排进 deferred registry，例如 `struct Registration_<FileID>` 与 `static Lumina::FRegisterCompiledInInfo <FileID>_Register_Static_Initializer(...)`（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` 中的 `EmitStaticRegistration`）。
8. 引擎启动时 `ProcessNewlyLoadedCObjects()` 处理这些注册，构造 `CStruct` / `CClass` / `CEnum` 元数据对象并放进对象哈希（`Engine/Source/Runtime/Source/Core/Object/ObjectBase.cpp`；调用点在 `Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）。
9. 此后序列化调用 `CStruct::SerializeTaggedProperties`，编辑器构造 `FPropertyTable`，C# 按名字解析 `LuminaSharp_*` thunk；三条消费链都不再关心源码长什么样。

## 3. 标记宏

宏定义都在 `Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`；libclang 解析时通过 `-DREFLECTION_PARSER` 全部退化为空宏。

| 宏 | 真实定义 | 用在 | 作用 |
| --- | --- | --- | --- |
| `REFLECT(...)` | `#define REFLECT(...)`（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`） | `class` / `struct` / `enum class` 声明之前 | 声明这是一个被反射的类型；specifier 由 Reflector 解析 |
| `GENERATED_BODY(...)` | `CONCAT4(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)`（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`） | 被反射 `class` / `struct` 体内 | 把生成头文件里按「行号」命名的宏展开成类体内容；`enum` 不用 |
| `PROPERTY(...)` | `#define PROPERTY(...)`（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`） | 被反射类型的非静态字段之前 | 把字段注册成 `FProperty` |
| `FUNCTION(...)` | `#define FUNCTION(...)`（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`） | 成员函数或静态函数之前 | 把函数注册成 `FFunction` 并生成 thunk |
| `SCRIPT_EXPORT(...)` | `#define SCRIPT_EXPORT(...)`（`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`） | 自由函数之前 | 只生成 C# 绑定与导出 thunk，不要求 generated.h |

`REFLECT` 能接受的 specifier 由一张权威表给出：`Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h`。例如 `Component` 的文档是「Marks the struct as an ECS component. Registers component meta.」，`MinimalAPI` 表示只导出 `StaticStruct()` / `StaticClass()`，`ReflectedName` 用于别名与模板实例化。`PROPERTY` / `FUNCTION` 各有自己的 specifier 表，同一文件里每行末尾都写明了消费者（`Reflector`、`Runtime`、`Editor`、`Script`）。

一个真实的最小示例，enum 与 struct 来自 `Engine/Source/Runtime/Source/AI/Navigation/NavTypes.h`：

```cpp
REFLECT()
enum class ENavArea : uint8
{
    Null    = 0,
    Ground  = 1,
    Water   = 2,
    Door    = 3,
    Danger  = 4,
    User0   = 16,
};

REFLECT()
struct RUNTIME_API FNavBuildSettings
{
    GENERATED_BODY()

    /** Voxel cell size in world units. Smaller = sharper edges, much slower bake (cubic cost). */
    PROPERTY(Editable, Category = "Voxel", ClampMin = 0.05f)
    float CellSize = 0.30f;
};
```

反射类与函数来自 `Engine/Source/Runtime/Source/World/DebugDrawLibrary.h`：

```cpp
REFLECT()
class RUNTIME_API CDebugDrawLibrary : public CFunctionLibrary
{
    GENERATED_BODY()

public:

    FUNCTION()
    static void DrawLine(CWorld* World, FVector3 Start, FVector3 End, FVector4 Color,
        float Thickness = 1.0f, float Duration = -1.0f);
};
```

命名约定由 `CONTRIBUTING.md` 规定：反射类用 `C` 前缀、反射结构体用 `S` 前缀、枚举用 `E` 前缀；Reflector 会对不以 `E` 开头的反射枚举给出 `BadTypePrefix` 警告（`Engine/Applications/Reflector/Source/Reflector/Clang/Visitors/ClangVisitor_Enum.cpp`）。

### 3.1 `REFLECTION_PARSER` 的作用

`Engine/Applications/Reflector/Source/Reflector/Clang/ClangParser.cpp` 在 libclang 参数里追加 `-DREFLECTION_PARSER`。这个宏的作用是让同一份引擎头文件在「被编译器编译」和「被 Reflector 解析」两种场景下都能通过：

- `Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h` 在 `REFLECTION_PARSER` 下把 `GENERATED_BODY` / `REFLECT` / `PROPERTY` / `FUNCTION` / `SCRIPT_EXPORT` 全部定义为空。
- `Engine/Source/Runtime/Source/ModuleAPI.h` 在 `REFLECTION_PARSER` 下把 `LUMINA_SCRIPT_API` 定义为空；注释写明原因是「The libclang frontend never sees `__declspec`」。
- `Engine/Source/Runtime/Source/Platform/Platform.h` 在 `REFLECTION_PARSER` 下把各种属性宏（`FORCEINLINE`、`NODISCARD`、`LIKELY` 等）替换成解析器能理解的形式，注释解释了不这样做时 `FORCEINLINE FVector3 GetSize()` 会被错误恢复成一个 int property。
- `Engine/Applications/Reflector/Source/Reflector/Clang/ClangParser.cpp` 还会把构建系统传入的每个 `<NAME>_API` 用 `-D<NAME>_API=` 重新定义为空，避免 libclang 看到 dllimport/dllexport。

## 4. Reflector 工具

### 4.1 入口与输入

入口是 `Engine/Applications/Reflector/Source/Reflector.cpp` 的 `main`。它要求第一个命令行参数是 `Reflection_Files.json` 的路径，用 `nlohmann/json` 解析后读取以下字段（键名与 `Engine/Applications/Reflector/Source/Reflector.cpp` 中读取的字段一致）：

| JSON 键 | 含义 |
| --- | --- |
| `WorkspaceName` / `WorkspacePath` | 工作区名与输出根 |
| `Projects[].Name` / `Projects[].Path` | 模块名与模块目录 |
| `Projects[].ReferenceOnly` | 仅作为类型来源解析，本身不生成代码 |
| `Projects[].GeneratedDir` | 该模块生成 C++ 的落地目录 |
| `Projects[].PrecompiledHeader` | 生成源文件首行包含的 PCH；为空表示不包含 |
| `Projects[].CSharpBindingsDir` | 该模块 C# 绑定的路由目录 |
| `Projects[].RouteTypeBindings` | 是否把类型绑定也路由到 `CSharpBindingsDir` |
| `Projects[].IncludeDirs` / `Definitions` / `ForceIncludes` | 传给 libclang 的 include 路径、宏与强制包含 |
| `Projects[].Files` | 参与解析的头文件列表 |

构建侧写出的字段与此一一对应（`Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs`）；`IsParseableHeader` 会排除 `.inl` / `.ipp`，因为把 include 片段当独立文件解析会误读 X-macro。构建工具实际传给 Reflector 的参数是 `<InputFile> -strict-parse`，并且把该 action 标记为 `bCanExecuteInParallel = false`，因为「The generator holds every header in memory and is already internally parallel, so a second instance fights it.」（`Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs`）。

### 4.2 libclang 解析

`ClangParser` 不会逐个解析头文件，而是先生成一个 amalgamation 文件，把每个可能带反射的头文件 `#include` 进去，再用一次 `clang_parseTranslationUnit2` 解析（`Engine/Applications/Reflector/Source/Reflector/Clang/ClangParser.cpp`）。关键参数包括：

```cpp
AppendArg("-x");
AppendArg("c++");
AppendArg("-std=c++23");
AppendArg("-march=x86-64-v3");
AppendArg("-DREFLECTION_PARSER");
```

解析选项使用 `CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing`。宏记录由 `Engine/Applications/Reflector/Source/Reflector/Clang/Visitors/ClangVisitor_Macro.cpp` 收集，它只认这五个名字：`PROPERTY`、`FUNCTION`、`REFLECT`、`GENERATED_BODY`、`SCRIPT_EXPORT`。

### 4.3 校验

在生成任何代码之前，`Engine/Applications/Reflector/Source/Reflector.cpp` 会做三类校验：

- 每个含反射宏的头文件必须以 `<Stem>.generated.h` 作为**最后一个** include；错名、缺失、或后面还有其它 include 都会产生 `WrongGeneratedHeader` / `MissingGeneratedHeader` / `GeneratedHeaderNotLast` 错误。
- 每个 `PROPERTY` 的类型必须已经 `REFLECT`，否则报 `UnreflectedPropertyType`，错误信息直接建议「Give that type a `REFLECT()` + `GENERATED_BODY()`, or remove the `PROPERTY()` macro from this field.」。
- 头文件循环包含在并行线程里由 `HeaderIncludeGraph` 检测，报 `CircularHeaderInclude`，并且「A cycle makes every later diagnostic noise, so it ends the run before anything is generated.」。

### 4.4 输出与增量

输出命名与目录由 `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` 的这几个函数固定：

- 生成头：`<GeneratedDir>/<FileName>.generated.h`
- 生成源：`<GeneratedDir>/<FileName>.generated.cpp`
- C# 绑定：路由时为 `<CSharpBindingsDir>/<FileName>.generated.cs`，否则为 `<WorkspacePath>/Intermediates/CSharpBindings/<Module>/<FileName>.generated.cs`

`GeneratedDir` 由构建系统显式传入（`Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs` 的 `GeneratedDir = Module.GeneratedCodeDirectory`），因此目录布局是构建系统的决定，而不是固化在工具里的约定（`Engine/Tools/LuminaBuildTool/README.md` 的 Reflection 一节）。

增量有两层：

1. `FReflectedHeader` 构造时比较源文件时间、Reflector 可执行文件时间与上次生成结果的时间，决定 `bDirty`（`Engine/Applications/Reflector/Source/Reflector/ReflectionCore/ReflectedHeader.cpp`）。
2. 即使判定为 dirty，落盘前还会做**内容比较**：`WriteTextFileIfChanged` 只在字节不同时才写（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`），注释写明原因是「Rewriting a generated header recompiles every TU that includes it, so skip an unchanged write.」。`Engine/Tools/LuminaBuildTool/README.md` 同样说明「Reflector 只在内容变化时才重写生成的文件」。

## 5. 生成的代码

> **本节内容是如何得到的**：仓库中**不包含任何已生成的产物**（`find . -name '*.generated.*'` 无命中，`Intermediates/` 下也没有），因此下面的文件结构、写出顺序与符号形态是从生成器自身的 emitter 代码（`CodeGenerator.cpp`、`ReflectionCore/ReflectedStruct.cpp`、`ReflectedEnum.cpp`、`ReflectedClass.cpp`、`CodeGeneration/ReflectionNames.h`）重建出来的，而不是与真实产物逐字节对照所得。构建一次引擎后即可用 `Intermediates/Reflection/` 下的实际文件核对；若发现出入，以生成器代码为准。

### 5.1 文件命名

| 产物 | 规则 | 来源 |
| --- | --- | --- |
| `<File>.generated.h` | 与源头文件同 stem，由 `MakeGeneratedHeaderPath` 拼出 | `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` |
| `<File>.generated.cpp` | 同上 | `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` |
| `<File>.generated.cs` | 同上，可路由 | `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` |
| `ReflectionUnity_<N>.gen.cpp` | 每个模块**固定写出 8 个 shard 文件**；空 shard 写成 stub | `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`、`Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs` |

注意区分两个数字：**shard 文件数固定为 8**，而**参与分配的有效分桶数**是 `ShardCountFor()` 算出的 `1..8`（源文件数 ≤ 20 时为 1，之后每 20 个加一桶，上限 8）。源文件只落进有效分桶，其余 shard 写成 stub。详见 [§5.5](#55-unity-shard-与进入构建)。

`kUnityShardCount` 在 `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` 中定义为 `8`，并注明必须与 `LuminaConfig.ReflectionUnityShardCount` 保持一致；构建侧的对应常量是 `ReflectionStep.UnityShardCount = 8`，注释写「Must match `kUnityShardCount` in `Reflector/CodeGeneration/CodeGenerator.cpp`」。

### 5.2 `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/ReflectionNames.h` 固定的符号名

`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/ReflectionNames.h` 是所有生成符号名的唯一来源，主要规则如下：

| 符号模式 | 生成函数 | 例子 |
| --- | --- | --- |
| `Construct_<Kind>_<Friendly>` | `Names::ConstructFunction` | `Construct_CStruct_Lumina_FNavBuildSettings` |
| `Registration_Info_<Kind>_<Friendly>` | `Names::RegistrationInfo` | `Registration_Info_CStruct_Lumina_FNavBuildSettings` |
| `<Construct>_Statics` | `Names::StaticsStruct` | `Construct_CStruct_Lumina_FNavBuildSettings_Statics` |
| `<Friendly>_Metadata` | `Names::MetadataArrayForType` | `Lumina_FNavBuildSettings_Metadata` |
| `<PropertyName>_Metadata` | `Names::MetadataArrayForProperty` | `CellSize_Metadata` |
| `<Type>_<Func>_Parms` | `Names::FunctionParmsStruct` | 调用帧结构体 |
| `Thunk_<Type>_<Func>` | `Names::FunctionThunk` | `Thunk_CDebugDrawLibrary_DrawLine` |
| `Func_<Func>` | `Names::FunctionParamsStatic` | `Func_DrawLine` |
| `LRT_XREF_<FnName>` | `Names::EmitGuardedCrossModuleDecl` | 跨模块 `Construct_*` 前向声明的去重 guard |

`FriendlyFromParts` 在 namespace 与类型名之间始终插入 `_`，注释解释了原因：`IMPLEMENT_CLASS` 用 `CONCAT4(Registration_Info_CClass_, TNamespace, _, TClass)` 构造同一个符号，未命名空间时也会多出一个分隔符。`ProjectApiMacro` 把模块名转成 `<NAME>_API`，`ScriptPackage` 把 `Runtime` 特判为 `/Script/Engine`，其它模块为 `/Script/<ProjectName>`。

### 5.3 `.generated.h` 的内容

`WriteHeaderContent` 按固定顺序写出（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`）：

- `#pragma once` 与「Generated by Lumina Reflection Tool - DO NOT EDIT.」横幅；
- `#include "Core/Reflection/ReflectedTypeAccessors.h"`；
- 双重包含检查：`#ifdef <FileID>_generated_h` 时报 `#error Already included, missing #pragma once`；
- 每个类型调用 `DefineInitialHeader`：前向声明、`Construct_*` 声明、enum 的 `template<> Lumina::CEnum* StaticEnum<...>()`；class 额外生成 `<FileID>_<Line>_CLASS` 宏块，内含 `DECLARE_CLASS` / `DEFINE_CLASS_FACTORY` / `DECLARE_SERIALIZER`；
- 每个类型调用 `DefineSecondaryHeader`：按 `GENERATED_BODY` 所在行号定义 `<FileID>_<Line>_GENERATED_BODY` 宏；struct 注入 `static CStruct* StaticStruct();` 与可选的 `using Super = ...;`，class 注入 `public:` / `<FileID>_<Line>_CLASS` / `private:`；
- 最后 `#undef CURRENT_FILE_ID` 再 `#define CURRENT_FILE_ID <FileID>`，供 `GENERATED_BODY` 展开使用。

enum 不生成 `GENERATED_BODY`，`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedEnum.cpp` 的注释写「Enums have no GENERATED_BODY, since StaticEnum is a specialization declared in the header.」；enum 的实现里会写出 `template<> Lumina::CEnum* StaticEnum<QualifiedName>()`。

### 5.4 `.generated.cpp` 的内容

`WriteSourceContent` 先写模块 PCH（`#include "<PrecompiledHeader>"`）与源头文件，然后固定包含 `Core/Object/Class.h`、`Core/Object/ObjectCore.h`、`Core/Reflection/Type/Function.h` 等。之后：

- `EmitCrossModuleReferences` 为每个类型和跨模块属性/参数写出带 `LRT_XREF_` guard 的 `Construct_*` 前向声明；
- 每个类型调用 `DeclareImplementation`：class 用 `IMPLEMENT_CLASS(Namespace, Name)`，struct 定义 `<Construct>_Statics` 结构体、`FStructParams`、属性参数数组 `PropPointers` 与 `Construct_CStruct_*`；class 定义 `FClassParams` 与 `Construct_CClass_*`；enum 定义 `FEnumParams` 与枚举值数组；
- 每个反射函数生成调用帧结构体 `<Type>_<Func>_Parms` 与 `Thunk_<Type>_<Func>`；参数用 `offsetof` 描述，`FFunctionParams` 里记录 thunk；
- 静态注册由 `EmitStaticRegistration` 写出。

静态注册的真实形态（对应 `EmitStaticRegistration` 的写出顺序，符号名取自 `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/ReflectionNames.h`）：

```cpp
struct Registration_<FileID>
{
    static constexpr Lumina::FStructRegisterCompiledInInfo StructInfo[] = {
        { Construct_CStruct_Lumina_FNavBuildSettings, TEXT("FNavBuildSettings") },
    };
};

static Lumina::FRegisterCompiledInInfo <FileID>_Register_Static_Initializer(
    nullptr, 0,
    nullptr, 0,
    Registration_<FileID>::StructInfo, std::size(Registration_<FileID>::StructInfo));
```

这三类 info 的真实字段定义在 `Engine/Source/Runtime/Source/Core/Object/ObjectBase.h` 的 `FClassRegisterCompiledInInfo` / `FStructRegisterCompiledInInfo` / `FEnumRegisterCompiledInInfo`；构造函数 `FRegisterCompiledInInfo` 会把它们交给 `RegisterCompiledInInfo` 重载。样例中 class 的注册项是 `{ Construct_CClass_*, TEXT("/Script"), TEXT("Name") }`，enum/struct 是两项 `{ Construct_*, TEXT("Name") }`，其余元数据字段缺省为零（`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedClass.cpp`、`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedEnum.cpp`、`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedStruct.cpp`）。

### 5.5 unity shard 与进入构建

`WriteUnityBuildFiles` 把排序后的 `<File>.generated.cpp` 名字按稳定哈希分到有效分桶里（`ShardCountFor`：源文件数 ≤ 20 时为 1 桶，之后每 20 个加一桶，上限 `kUnityShardCount` = 8），每个 shard 文件里是若干 `#include "<File>.generated.cpp"`。`Shards` 数组长度固定为 `kUnityShardCount`，循环无条件写出全部 8 个文件，空 shard 写成 stub，注释给出的理由是「Every shard must exist even at zero reflected types, or premake's static list points at missing files.」。

构建侧把 shard 直接当作编译输入：`ReflectionStep.CreateActions` 把属主模块的每个 shard 加入 `Action.ProducedItems`（非属主模块走 `OptionalProducedItems`），`Engine/Tools/LuminaBuildTool/Graph/TargetAssembler.cs` 把 `ReflectionStep.EnumerateUnityShardPaths(...)` 加进 `Module.GeneratedSourceFiles`。`Engine/Tools/LuminaBuildTool/README.md` 说明反射分片本身已经是 unity 文件，因此会被自动排除在普通 unity 合并之外。

## 6. 运行时元数据模型

### 6.1 类型对象

`Engine/Source/Runtime/Source/Core/Object/Class.h` 里的继承链是：

| 类型 | 职责 |
| --- | --- |
| `CField : CObject` | 类型名、`Size` / `Alignment`、`FMetaDataPair Metadata` |
| `CEnum : CField` | `TVector<TPair<FName, uint64>> Names`，`GetNameAtValue` / `GetEnumValueByName` / `IsBitmaskEnum()` |
| `CStruct : CField` | `OwnProperties` / `AllProperties`、`OwnFunctions` / `AllFunctions`、`SuperStruct`、`FPropertyArena`，`GetProperty()` / `FindFunction()` / `Link()` |
| `CClass : CStruct` | 工厂函数、CDO（`GetDefaultObject()`）、`GetSuperClass()` |

`CStruct::GetProperties()` 返回「its own properties in declaration order, then each super's」，`GetOwnProperties()` 只返回本类型声明的属性；`CStruct::Link()` 负责把 `AllProperties` / `AllFunctions` 展平，且必须在所有 `AddProperty` 之后运行。

### 6.2 `FProperty`

`FProperty` 是属性基类，定义在 `Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.h`。核心字段有 `Name`、`Offset`、`ElementSize`、`Flags`（`EPropertyFlags`）、`TypeFlags`（`EPropertyTypeFlags`）、`NumMetadata`、`DisplayName`、`OwnerStruct`、`MetadataEntries`。它提供：

- 读写：`GetValuePtr<T>()` / `SetValue()` / `GetValue()`，其中 `GetValuePtr` 的注释明确「UB if ValueType doesn't match the property type」；
- 生命周期：`ConstructValue` / `DestructValue` / `OwnsStorage`，让持有者在不知道具体类型的情况下驱动内存生命周期；
- 序列化：`Serialize(FArchive&, void*)`、`SerializeItem(IStructuredArchive::FSlot, ...)`、`NetSerialize(FNetArchive&, void*)`；
- 比较/拷贝：`Identical` / `CopyCompleteValue` / `..._InContainer`；
- 标志查询：`ShouldSerialize()`（`NoSerialize` 的反）、`IsEditorOnly()`、`IsReplicated()`、`IsEditable()`、`IsReadOnly()`、`IsVisible()`（`ReadOnly | Editable`）；
- 元数据：`GetMetadata(FStringView)` / `HasMetadata`，实现是大小写不敏感的顺序扫描（`Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.cpp`）。

属性类型由 `EPropertyTypeFlags` 枚举，单一来源是 `Engine/Source/Runtime/Source/Core/Object/EPropertyTypeFlags.inl`，顺序即 ABI（注释写明「Order IS the ABI: the enum values are the wire kind bytes the C# script schema sends」）。当前列表是：`Int8`、`Int16`、`Int32`、`Int64`、`UInt8`、`UInt16`、`UInt32`、`UInt64`、`Float`、`Double`、`Bool`、`Object`、`SoftObject`、`Class`、`Name`、`String`、`Enum`、`Vector`、`Struct`、`Optional`、`SubStruct`、`Delegate`、`InstancedStruct`、`Map`、`Entity`。

每种属性对应一个运行时类（`FClassProperty`、`FEnumProperty`、`FStructProperty`、`FArrayProperty`、`FMapProperty`、`FOptionalProperty`、`FSoftObjectProperty`、`FObjectProperty`、`FStringProperty`、`FNameProperty`、`FInstancedStructProperty`、`FSubStructProperty`、`FDelegateProperty`），都在 `Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/`。属性种类本身可插拔：`GetPropertyKindOps(EPropertyTypeFlags)` 返回 `FPropertyKindOps`（`Engine/Source/Runtime/Source/Core/Reflection/Type/PropertyRegistry.h`），其中 `Construct`、`GetMetadata`、`NumInnerParams` / `GetNumInnerParams` 决定容器内层怎么构造。

容器与递归支持由这些类的 `AddProperty` + `Serialize` 实现：

- `FArrayProperty` 持有一个 `Inner`，操作通过类型擦除的 `FVectorOps` 表，暴露 `GetNum` / `GetAt` / `PushBack` / `RemoveAt` / `Resize` 等；
- `FMapProperty` 持有 `KeyProperty` 与 `ValueProperty`，通过 `FMapOps` 表，`AddProperty` 注释强调写入顺序是「KEY first, then VALUE」的 ABI 契约；
- `FStructProperty::Serialize` 直接递归调用 `Struct->SerializeTaggedProperties`，所以嵌套结构体天然支持；
- `FOptionalProperty` 先写 engaged bool，再写 payload；
- `FInstancedStructProperty` 按运行时类型名查 `CStruct` 后递归 `SerializeTaggedProperties`。

`Engine/Source/Runtime/Source/Core/Object/ObjectCore.cpp` 的 `ConstructProperties` 从 params 数组**反向**读取：先构造当前属性，再按 `Ops.CountInnerParams(Param)` 递归构造 inner；`InitializeAndCreateFProperties` 从数组末尾开始，注释解释了原因（inner 是紧跟在后的 params 条目）。

### 6.3 `FFunction`

`FFunction`（`Engine/Source/Runtime/Source/Core/Reflection/Type/Function.h`）刻意**不是** `CObject`，而是一个放在所属 struct 的 `FPropertyArena` 里的普通类型。它包含：

- `Name`、`OwnerStruct`、`Params`、`ReturnParam`、`Thunk`；
- `FFunction::FNativeFuncPtr = void (*)(const FFunction&, void* Context, void* Frame)`；
- `InitializeFrame` / `DestructFrame` / `Invoke`，以及 `FFunctionFrame` 这个 128 字节内联、超出后走堆的调用帧；
- `EFunctionFlags`：`Static`、`Const`、`Virtual`、`Scriptable`、`ScriptCallable`、`HasReturn`、`ScriptImplemented`、`RpcBroadcast`、`RpcHost`、`RpcOwner`、`NetUnreliable`、`NetOwnerOnly`、`UIBind`。

参数之所以是 `FProperty`，注释给出的理由值得引用：「Parameters are FProperty because that is what makes every reflected type callable for free... Adding a property type does not teach this anything.」

### 6.4 静态访问器

生成代码给每个类型提供静态入口：

```cpp
// struct / class
T::StaticStruct()      // 返回 Lumina::CStruct*
T::StaticClass()       // 返回 Lumina::CClass*
// enum
StaticEnum<EnumType>() // 声明在 Core/Reflection/ReflectedTypeAccessors.h
```

`StaticStruct()` / `StaticClass()` 由 `Engine/Source/Runtime/Source/Core/Object/ObjectMacros.h` 的 `DECLARE_CLASS` / `IMPLEMENT_CLASS` 支撑；`IMPLEMENT_CLASS` 用 `CONCAT4(Registration_Info_CClass_, TNamespace, _, TClass)` 建出与 Reflector 一致的符号。`StaticEnum` 的模板声明在 `Engine/Source/Runtime/Source/Core/Reflection/ReflectedTypeAccessors.h`，生成头文件里显式实例化。

### 6.5 注册与最终化

翻译单元初始化时，生成的 `<FileID>_Register_Static_Initializer`（类型 `Lumina::FRegisterCompiledInInfo`）把 enum/class/struct 三张 info 表交给 `RegisterCompiledInInfo` 重载。`Engine/Source/Runtime/Source/Core/Object/ObjectBase.cpp` 的实现只是把构造函数压进三个 deferred registry：

```cpp
void RegisterCompiledInInfo(CStruct*(*RegisterFn)(), const FStructRegisterCompiledInInfo& Info)
{
    FStructDeferredRegistry::Get().AddRegistration(RegisterFn);
}
```

registry 是 `TDeferredRegistry<T>`（`Engine/Source/Runtime/Source/Core/Object/DeferredRegistry.h`），类型分别是 `FClassDeferredRegistry`、`FEnumDeferredRegistry`、`FStructDeferredRegistry`。它用 `ProcessedRegistrations` 水位线区分「已处理」与「新加载的模块」；`TruncateRegistrations` 用于卸载被拒绝的模块时丢弃尚未处理的注册。

真正构造元数据对象发生在 `ProcessNewlyLoadedCObjects()`（`Engine/Source/Runtime/Source/Core/Object/ObjectBase.cpp`）。它循环处理 pending registrants、加载 enum 与 struct、再按继承深度排序后为每个新 class 创建 CDO；`SettleDeferredRegistrations()` 是带 re-entrancy guard 的包装。`ConstructCStruct` / `ConstructCClass` / `ConstructCEnum` 在 `Engine/Source/Runtime/Source/Core/Object/ObjectCore.cpp`：`ConstructCStruct` 设置 `Size` / `Alignment` / `StructOps`，构造属性与函数，设置 super，最后调用 `FinalClass->Link()`；`ConstructCEnum` 逐个 `AddEnum`。

引擎启动时在若干模块加载阶段之后调用 `ProcessNewlyLoadedCObjects()`，真实调用点包括 `Engine/Source/Runtime/Source/Core/Engine/Engine.cpp` 的初始化序列（`ProcessNewlyLoadedCObjects(); BootMark("ProcessNewlyLoadedCObjects");` 及其后的每个 plugin loading phase）。

### 6.6 按名字或类型查找

- 编译期已知类型：直接用 `T::StaticStruct()` / `T::StaticClass()` / `StaticEnum<T>()`。
- 运行期只知道名字：`FindObject<CStruct>(FName)`，最终走 `FindObjectImpl(const FName&, CClass*)` → `FObjectHashTables::FindObject(Name, Class)`（`Engine/Source/Runtime/Source/Core/Object/ObjectCore.h`、`Engine/Source/Runtime/Source/Core/Object/ObjectCore.cpp`）。`TBaseStructure<FVector3>::Get()` 就是按名字 `"FVector3"` 查的（`Engine/Source/Runtime/Source/Core/Object/Class.cpp`）。
- 在类型内找成员：`CStruct::GetProperty(const FName&)` 会搜索完整继承链，`CStruct::FindFunction(const FName&)` 返回最近的声明。

## 7. 序列化

反射驱动序列化的主入口是 `CStruct::SerializeTaggedProperties(FArchive&, void*)`（`Engine/Source/Runtime/Source/Core/Object/Class/Struct.cpp`），`CObject::Serialize` 只是转调它。写路径遍历 `AllProperties`：

- `Current->ShouldSerialize()` 为 false（即 `PROPERTY(NoSerialize)`）的字段跳过；
- `Ar.IsCooking() && Current->IsEditorOnly()` 的字段在 cook 时跳过；
- 每个字段先写 `FPropertyTag { Type, Name, Size }`，再写值，最后回填 `Size`，所以读取端可以跳过不认识的字段；
- 读路径优先按顺序 O(1) 匹配，其次回退到线性搜索，再匹配 `Aliases` 元数据以兼容重命名；类型变了但双方都是数值类型时尝试转换。

`FPropertyTag` 的真实结构在 `Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/PropertyTag.h`，它按 `ELuminaEngineVersion::PACKAGE_NAME_TABLE` 区分新旧格式。

结构化/JSON 版本走另一个重载 `CStruct::SerializeTaggedProperties(IStructuredArchive::FRecord&, void*, const void* Defaults)`，逐个调用 `Current->SerializeItem(Record.EnterField(...), ...)`；`FJsonStructuredArchive::SaveStruct` / `LoadStruct` 是它的真实调用者（`Engine/Source/Runtime/Source/Core/Serialization/Structured/JsonStructuredArchive.cpp`）。

支持哪些容器与递归类型，按 `EPropertyTypeFlags` 分类：

| 种类 | 运行时类 | 关键实现 |
| --- | --- | --- |
| `Vector` | `FArrayProperty` | `Serialize` 写 `ElementCount` 与 element size 后逐个用 `Inner` 序列化；plain struct 有 schema-once 快路径（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/ArrayProperty.cpp`） |
| `Map` | `FMapProperty` | `Serialize` 写 `Count` 与 key/value size 后逐对序列化 key/value 两个 inner（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/MapProperty.cpp`） |
| `Optional` | `FOptionalProperty` | engaged bool + payload（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/OptionalProperty.cpp`） |
| `Struct` | `FStructProperty` | 递归 `Struct->SerializeTaggedProperties`（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/StructProperty.cpp`） |
| `InstancedStruct` | `FInstancedStructProperty` | 运行时类型 + 递归（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/InstancedStructProperty.cpp`） |
| `Object` / `SoftObject` | `FObjectProperty` / `FSoftObjectProperty` | 对象引用按 GUID 走 `FObjectProxyArchiver`（`Engine/Source/Runtime/Source/Core/Serialization/ObjectArchiver.cpp`） |
| `Enum` | `FEnumProperty` | 委托给 inner 的 `FNumericProperty`（`Engine/Source/Runtime/Source/Core/Reflection/Type/Properties/EnumProperty.h`） |
| 数值 / bool / string / name | `TProperty_Numeric` / `FStringProperty` / `FNameProperty` | `TProperty` 的 `Serialize` 直接 `Ar << value` |

按项目定制的方式：

- 字段级：`PROPERTY(NoSerialize)`、`PROPERTY(EditorOnly)`、`PROPERTY(Replicated)` / `PROPERTY(Sync)` 等；要新增属性种类则 override `FProperty::Serialize`。`PROPERTY` 的 specifier 语义以 `Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h` 为准。
- 类型级：`MakeStructOps<T>()` 会按 concept 探测 `Serialize` / `NetSerialize` / `CopyFrom` / `operator==` / `ToString` / `operator<` / `PostEditChange` 等成员，填进 `FStructOps`（`Engine/Source/Runtime/Source/Core/Object/Class/StructTraits.h`）。生成代码给每个 struct 生成 `GetStructOps()` 并放进 `FStructParams`，因此自定义类型序列化只需要在类型上写一个 `Serialize(FArchive&)`。
- 网络路径：`CStruct::NetSerializeProperties` 只遍历 `IsReplicated()` 的字段，`NetSerializeAll` 用于嵌套结构体，`NetSerializeReplicatedFlat` / `NetReadReplicatedMasked` 用 changed-field bitmask 做增量（`Engine/Source/Runtime/Source/Core/Object/Class/Struct.cpp`）。

## 8. 编辑器集成

编辑器属性树在 `Engine/Editor/Source/UI/Properties/PropertyTable.cpp` 的 `FPropertyTable::RebuildTree()` 中生成。它遍历 `Struct->GetProperties()`，只保留 `Current->IsVisible()` 为真的属性，然后按 `Category` 元数据构建 `FCategoryPropertyRow`：

- `Category` 缺省是 `General`；嵌套层级用竖线分隔，代码按 `category.find('|')` 逐段创建子 category（`Engine/Editor/Source/UI/Properties/PropertyTable.cpp`）。
- 每个叶节点的显示名来自 `FProperty::GetPropertyDisplayName()`；没有 `DisplayName` 元数据时由 `MakeDisplayNameFromName` 从字段名派生，bool 的 `b` 前缀会被去掉，并插入空格（`Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.cpp`）。
- tooltip 优先读 `ToolTip` 元数据，缺省退回 `GetPropertyDisplayName()`（`Engine/Editor/Source/UI/Properties/PropertyTable.cpp`）。

影响 UI 的真实元数据（键名全部来自 `Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h` 的 `LUMINA_PROPERTY_SPECIFIERS` 表）：

| 元数据 | 作用 |
| --- | --- |
| `Category` | details panel 分组；嵌套用竖线 |
| `DisplayName` / `ToolTip` | 标签与悬停文本 |
| `Editable` / `ReadOnly` | 是否显示、是否允许编辑；`FProperty::IsVisible()` 就是这两个标志 |
| `ClampMin` / `ClampMax` / `Delta` / `Units` | 数值控件的范围、步长与单位后缀 |
| `Color` / `NoDrag` / `Multiline` / `FilePath` | 颜色控件、禁用拖拽、多行文本、文件选择 |
| `Picker` / `AssetType` / `RowType` | 名字或字符串属性使用哪个 picker、限制 asset class / data table row struct |
| `NoReorder` / `NoResize` / `DefaultCollapsed` | 数组的拖拽手柄、增删按钮，以及结构体的初始折叠状态 |
| `EditCondition` / `EditConditionHides` | 按 `Prop`、`!Prop`、`Prop == Value`、`Prop != Value`（`&&` / `\|\|` 连接）禁用或隐藏属性；解析器是 `FPropertyEditCondition`（`Engine/Editor/Source/UI/Properties/PropertyTable.h` / `Engine/Editor/Source/UI/Properties/PropertyTable.cpp`） |
| `RequiresRecook` | 编辑后触发 asset recook |

控件选择有两条路径：

- 叶子属性按 `FProperty::GetType()` 分派到 `FNumericPropertyCustomization<T>`、`FBoolPropertyCustomization`、`FEnumPropertyCustomization`、`FStringPropertyCustomization` 等（`Engine/Editor/Source/UI/Properties/PropertyTable.cpp` 的 `MakeLeafPropertyCustomization`）。
- `FPropertyCustomizationRegistry` 按 `CStruct` 名字注册整类型定制（`Engine/Source/Runtime/Source/Core/Reflection/PropertyCustomization/PropertyCustomization.h`）；编辑器在 `Engine/Editor/Source/UI/EditorUI.cpp` 里为 `TBaseStructure<FVector3>::Get()->GetName()`、`FAssetRef::StaticStruct()->GetName()`、`SDataTableRowHandle::StaticStruct()->GetName()` 等注册。

组件选择器则直接读类型级元数据：`FSceneEditorTool` 用 `Struct->HasMeta("HideInComponentList")` 决定是否出现在 add-component 列表，用 `Struct->HasMeta("Category")` / `GetMeta("Category")` 分组（`Engine/Editor/Source/UI/Tools/FSceneEditorTool.cpp`）。`REFLECT(Component, ...)` 还会在生成的 `Construct_CStruct_*` 里调用 `::Lumina::Meta::RegisterComponentMeta<T>()`（`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedStruct.cpp` 的 `EmitComponentMetaRegistrations`），把 ECS 操作表注册给运行时。

需要注意：`PROPERTY` 的**普通元数据**（`Category`、`ToolTip` 等）通过 `METADATA_PARAMS` 写出，而 `Engine/Source/Runtime/Source/Core/Object/ObjectMacros.h` 在非 `WITH_EDITOR` 构建里把 `METADATA_PARAMS` 展开为空。因此这类编辑器元数据在 Shipping 里不存在；而 `NoSerialize` / `Replicated` 等是 `EPropertyFlags` 位，运行时依然有效。

## 9. C# 绑定生成

### 9.1 生成与路由

Reflector 为每个含反射类型的头文件写一个 `.generated.cs`（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` 的 `GenerateCSharpFile`）。默认目录是 `Intermediates/CSharpBindings/<Module>/`；插件或游戏模块可以把类型绑定路由到自己脚本程序集的 `CSharpBindingsDir`。`Engine/Tools/LuminaBuildTool/README.md` 说明：生成的 C# 绑定默认写到 `Intermediates/CSharpBindings`，并会编译进 `LuminaSharp`；插件或游戏模块可以设置 `CSharpBindingsDirectory`，改为写入自己的脚本程序集。

生成的 C# wrapper 通过 `[global::LuminaSharp.NativeCall(Module = "...", EntryPoint = "...")]` 声明 partial 方法，方法体由 `Engine/Source/LuminaSharp.Generators/NativeCallGenerator.cs` 生成。`NativeCallAttribute` 的 `Module` 缺省是 `LuminaNative`，`EntryPoint` 缺省是 `LuminaSharp_<MethodName>`（`Engine/Source/LuminaSharp.Generators/NativeCallGenerator.cs` 的 `DefaultModule` / `entry ??= "LuminaSharp_" + method.Name`）。

对于 C# 脚本类型（`[Property]` / `[ScriptFunction]`），系统不走 Reflector 的 C++ 生成，而是在运行期由 `ScriptSchemaCodec` 解析 managed schema，再由 `AppendScriptPropertiesToClass` 把字段追加成真实的 `FProperty`（`Engine/Source/Runtime/Source/Scripting/ScriptStruct.h`）。它的注释解释了这样做的目的：让脚本属性「be drawn by the stock FPropertyTable, serialized by SerializeTaggedProperties, and covered by undo / prefab overrides / replication」。

### 9.2 thunk 命名与按名字解析

Reflector 生成的 C++ export 都以 `LuminaSharp_` 开头（`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CSharpBindingEmitter.cpp`）：

| thunk | 用途 |
| --- | --- |
| `LuminaSharp_Call_<Friendly>_<Function>` | 反射成员/静态函数的实例调用 |
| `LuminaSharp_Base_<Friendly>_<Function>` | `REFLECT(Scriptable)` 基类转发 shim |
| `LuminaSharp_VecOps_<Friendly>_<Member>` | `TVector<T>` 的 ops 表导出 |
| `LuminaSharp_Count_<Friendly>_<Member>` / `LuminaSharp_GetAt_<Friendly>_<Member>` | 非 blittable 数组的逐元素读取 |
| `LuminaSharp_Export_<FriendlyQualified>` | `SCRIPT_EXPORT` 自由函数 |
| `LuminaSharp_PropertyOffsetByName` / `LuminaSharp_FindProperty` | 生成 wrapper 解析字段 offset / `FProperty*` |

生成代码给这些导出加的是 `LUMINA_SCRIPT_API`，而不是模块自己的 `<NAME>_API`。`Engine/Source/Runtime/Source/ModuleAPI.h` 的注释说明了原因：

```cpp
// C#<->native interop thunks (the reflector-generated LuminaSharp_* thunks + the hand-written
// LUMINA_DOTNET_EXPORT ones) are resolved by NAME at runtime via GetProcAddress / NativeLibrary.TryGetExport,
// never linked at compile time. They must therefore live in an export table in EVERY build mode...
#define LUMINA_SCRIPT_API DLL_EXPORT
```

C# 侧 `NativeBindings.Resolve(Module, EntryPoint)` 先通过 `Host.ModuleHandle(Module)` 拿到模块句柄，再调用 `NativeLibrary.TryGetExport`（`Engine/Source/LuminaSharp/Interop/NativeBindings.cs`）。`Host.ModuleHandle` 把 `LuminaNative` 映射到宿主 Runtime 模块，其它模块名走 `LuminaSharp_ResolveModuleHandle`（`Engine/Source/LuminaSharp/Interop/Host.cs`）。按名字解析而不是直接链接，是因为生成代码分别编译进 native 模块和 managed 程序集，且 monolithic Shipping 会把对象文件 whole-archive 进 exe；两边没有同一个链接步骤。

### 9.3 `ManagedTypeRegistry`

`Engine/Source/Runtime/Source/Scripting/ManagedTypeRegistry.h` 定义了 `IManagedTypeCompiler` 与 `FManagedTypeRegistry`：每个 stage 有 `Compile` / `PreUnload` / `UnloadAborted`，注册顺序就是执行顺序，卸载时反向执行。引擎内置两个 stage（`Engine/Source/Runtime/Source/Scripting/ManagedTypeStages.cpp`）：

- `ScriptableClasses`：调用 `FScriptableRegistry::RefreshMintedClasses(Definitions)`，注释写「First, because everything after it reads the classes this produces.」；
- `DataStructs`：调用 `FScriptDataStructRegistry::Get().Refresh()`，注释写「After the class minting, since both read the generation that just loaded.」。

### 9.4 布局校验（layout checks）解决的真实问题

C# 里有一批手写的 blittable 镜像 struct 会和 native 类型按值互传。两边独立声明，字段类型漂移或对齐差异会导致 `sizeof` 不一致，进而按错误长度读写 native 内存。Lumina 的做法是在 bootstrap 时逐个核对：

- native 侧每个镜像类型用 `LE_REGISTER_LAYOUT(Key, Type)` 自注册 `sizeof`（`Engine/Source/Runtime/Source/Scripting/DotNet/LayoutRegistry.h`），导出 `LuminaSharp_Layout_GetSize`；
- managed 侧每个镜像带 `[NativeLayout("key")]`，`LayoutValidator.ValidateAll()` 用 `Unsafe.SizeOf<T>()` 与 native 返回值比较；不一致就 `return false`，`Host.Bootstrap` 直接返回非零，拒绝启动 C#（`Engine/Source/LuminaSharp/Interop/LayoutValidator.cs`、`Engine/Source/LuminaSharp/Interop/Host.cs`）。错误文本写明「refusing to start C# (a mismatched blittable layout or enum would corrupt memory)」。

同一校验还覆盖枚举与标志位：`EPropertyType` 的每个枚举值、成员数与 native `EPropertyTypeFlags` 对齐；`EPropertyFlags` / `EScriptParamFlags` 的每个位与 native 对齐（`Engine/Source/LuminaSharp/Interop/LayoutValidator.cs`，native 实现导出在 `Engine/Source/Runtime/Source/Scripting/DotNet/LayoutRegistry.cpp`）。这些值会作为 script schema 的 wire kind byte 和复制标志使用，漂移不会崩溃，而是静默地少复制一个字段，所以必须在启动时失败。

### 9.5 `ExportSignature` 的作用

`Engine/Source/Runtime/Source/Scripting/DotNet/ExportSignature.h` 用一个跨语言宽度表解决「函数签名漂移但名字没变」的问题：

```cpp
#define LUMINA_DOTNET_SIG(Name) \
    { "LuminaSharp_" #Name, ::Lumina::DotNet::SignatureSize(&LuminaSharp_##Name) }
```

每个 `LUMINA_DOTNET_EXPORT` 所在文件用 `LUMINA_DOTNET_SIGNATURES(...)` 登记这些导出；`SignatureSize` 是返回值和每个参数 `sizeof` 之和，引用按一个指针宽计算。C# 侧 `NativeBindings.Resolve(Module, EntryPoint, ExpectedSignature)` 会调用 `LuminaSharp_ExportSignature` 查询 native 声明宽度，不一致就打印错误并**丢弃该绑定**（`Engine/Source/LuminaSharp/Interop/NativeBindings.cs`）。注释概括为「A width both sides derive from their own declaration, so a drifted signature fails at bind, not mid-call.」

### 9.6 `CSharpBindingsDirectory` 的定制能力

`ModuleRules.CSharpBindingsDirectory` 是模块级设置（`Engine/Tools/LuminaBuildTool/Configuration/ModuleRules.cs`），`bRouteCSharpTypeBindings` 决定是否连类型绑定也一起路由。构建工具在 `Engine/Tools/LuminaBuildTool/Graph/TargetAssembler.cs` 中自动处理：

- 插件模块若未显式设置，默认路由到 `<Plugin>/Scripts/Generated`；
- 非引擎模块（游戏项目模块）默认路由到 `<Project>/Game/Scripts/Generated`，并置 `bRouteCSharpTypeBindings = true`。

Reflector 侧读取 `CSharpBindingsDir` 与 `RouteTypeBindings` 决定 `.generated.cs` 的落地目录（`Engine/Applications/Reflector/Source/Reflector.cpp`、`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`），并对路由目录做孤儿文件清扫。

## 10. 热重载

脚本侧热重载是完整实现的：

1. 文件变动由 watcher 调 `NotifyScriptSourceChanged(FStringView)`；它会判断扩展名、合并事件突发，并 latch 一次 reload（`Engine/Source/Runtime/Source/Scripting/DotNet/DotNetHost.h` 的注释说明了这个策略）。
2. 引擎帧开始时调用 `ProcessPendingScriptReload()`（真实调用点 `Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`），而不是在 draw 中执行。
3. `ReloadScripts()` 走 `LoadScriptUnitsCore(BuildScriptUnits(), /*bEditorFollowups*/true)`（`Engine/Source/Runtime/Source/Scripting/DotNet/DotNetHost.cpp`）。编辑器里还有后台编译路径 `BeginBackgroundReload` / `FinishBackgroundReload`：编译失败时注释写明「The live generation was never touched, so a failed compile leaves every script and binding running.」。
4. 编译成功后切换 generation。`FManagedTypeRegistry` 的每个 stage 在此之前 `PreUnload`，失败时 `UnloadAborted` 回滚（`Engine/Source/Runtime/Source/Scripting/ManagedTypeRegistry.h`）。
5. 脚本对象通过 `FObjectReinstancer` 迁移到新 minted class：`Reinstancer.SetPostReplaceHook` 里恢复普通字段的值、调用 `Scripting::ResetSkipHotReloadProperties(New)`，并让 `CEntityScript` 走 `ContinueLifecycleOf` 而不是重新触发 `OnAttach` / `OnReady`（`Engine/Source/Runtime/Source/Scripting/ScriptableObject.cpp`）。

C# 属性可以用 `[SkipHotReload]`（`Engine/Source/LuminaSharp/Scripting/Attributes.cs`）或 native 侧的 `PROPERTY(SkipHotReload)` 声明「重载后回到默认值而不是沿用旧值」；native 实现是 `CScriptStruct::ResetHotReloadFields` 与 `ResetSkipHotReloadProperties`（`Engine/Source/Runtime/Source/Scripting/ScriptStruct.h`）。

C++ 侧**没有**通用模块热重载。插件浏览器里明确写着「Module hot-reload isn't implemented, so changes only take effect on next launch.」（`Engine/Editor/Source/UI/Tools/PluginBrowserEditorTool.cpp`）。C++ 反射代码的重生成由构建系统完成：头文件变化后重新运行 Reflector、重编 unity shard；生成的 `.generated.h` 只在内容变化时重写，以尽量减少重编译范围。与模块卸载直接相关的反射侧设施是 deferred registration 的快照/回滚：加载 DLL 会立刻运行静态注册，若模块随后被拒绝，`RollbackDeferredRegistrations` 会丢弃尚未处理的注册，避免之后调用已卸载模块里的函数指针（`Engine/Source/Runtime/Source/Core/Object/ObjectBase.h`）。

## 11. 常见问题与限制

| 现象 / 限制 | 原因与真实检查 |
| --- | --- |
| `Cannot find .generated.h` | 含反射宏的头文件必须以 `<Stem>.generated.h` 结尾；Reflector 报 `MissingGeneratedHeader`（`Engine/Applications/Reflector/Source/Reflector.cpp`）。`README.md` 也提到 Visual Studio 有时需要第二轮构建才能拾取生成文件。 |
| 生成头必须最后 include | 反射头文件后面若还有 include，报 `GeneratedHeaderNotLast`（`Engine/Applications/Reflector/Source/Reflector.cpp`）。 |
| `Property 'X' ... is not reflected` | `PROPERTY` 引用的类型必须本身 `REFLECT` + `GENERATED_BODY`，否则报 `UnreflectedPropertyType`（`Engine/Applications/Reflector/Source/Reflector.cpp`）。 |
| 反射枚举必须有显式底层类型 | `Engine/Applications/Reflector/Source/Reflector/Clang/Visitors/ClangVisitor_Enum.cpp` 在 `clang_getEnumDeclIntegerType` 无效时报错，要求写成 `enum class E : uint8`。匿名 enum 也会被拒绝。 |
| 反射枚举命名 | 不以 `E` 开头只警告，不阻止生成（`Engine/Applications/Reflector/Source/Reflector/Clang/Visitors/ClangVisitor_Enum.cpp`）。 |
| `CSharpValueMirror` 类型不能做 `PROPERTY` | 这种类型只为 C# marshalling 存在、没有成员，报 `ScriptOnlyPropertyType`（`Engine/Applications/Reflector/Source/Reflector.cpp`）。 |
| 含未反射字段的 struct 不能按值镜像到 C# | `FReflectedType::bHasUnreflectedFields` 的注释说明这样的布局不完整，不能做 flat by-value struct（`Engine/Applications/Reflector/Source/Reflector/Types/ReflectedType.h`）。 |
| `FProperty::GetValuePtr<T>` 类型不匹配是 UB | 头文件注释「UB if ValueType doesn't match the property type.」（`Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.h`） |
| Shipping 没有编辑器元数据 | `PROPERTY` 的普通 metadata 经 `METADATA_PARAMS` 输出，非 `WITH_EDITOR` 构建展开为空（`Engine/Source/Runtime/Source/Core/Object/ObjectMacros.h`）。 |
| `ReferenceOnly` 模块不生成 | 只参与解析、不产出文件（`Engine/Applications/Reflector/Source/Reflector.cpp`、`Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp`）。 |
| unity shard 数量不能随便改 | 必须同时改 `Engine/Applications/Reflector/Source/Reflector/CodeGeneration/CodeGenerator.cpp` 的 `kUnityShardCount` 与 `ReflectionStep.UnityShardCount`，两处注释互相指向对方。 |
| 反射分片不能再被普通 unity 合并 | `Engine/Tools/LuminaBuildTool/README.md` 把 reflection shard 列在「永远不能合并」的源文件类型里，因为它们本身就是 unity 文件。 |
| 插件模块不支持热重载 | `FPluginBrowserEditorTool` 的帮助文本写明改动在下次启动才生效（`Engine/Editor/Source/UI/Tools/PluginBrowserEditorTool.cpp`）。 |

## 12. 相关文档

- [`EngineOverview.md`](EngineOverview.md) —— 仓库布局、模块与目标、启动流程；其中也有一份子系统与源码入口对照表。
- [`ECSGuide.md`](ECSGuide.md) —— `REFLECT(Component, ...)`、组件序列化与反射元数据在 ECS 侧的使用。
- [`RenderingArchitecture.md`](RenderingArchitecture.md) —— 渲染架构文档，主题与本文相互独立。
- [`../Engine/Tools/LuminaBuildTool/README.md`](../Engine/Tools/LuminaBuildTool/README.md) —— Reflection 一节给出构建系统如何调用 Reflector、输入/输出路径与增量策略。
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) —— 反射类型的命名约定（`C` / `S` / `E` 前缀）与代码风格。
- [`../DEPENDENCIES.md`](../DEPENDENCIES.md) —— LLVM/Clang 19 是反射代码生成的依赖来源。
