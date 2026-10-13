<div align="center">

# 为 Lumina Engine 贡献代码

**携手共建游戏引擎的未来。**

[![Discord](https://img.shields.io/discord/1193738186892005387?label=Discord&logo=discord)](https://discord.gg/xQSB7CRzQE)
[![License](https://img.shields.io/github/license/mrdrelliot/lumina)](LICENSE)

</div>

---

## 我们的理念

**贡献是每个人的权利。** Lumina 是一个以知识共享为原则、由社区驱动的项目。我们相信，每一份贡献，无论多么微小，都有助于推进我们对游戏引擎架构的集体理解。

### 我们欢迎

- Bug 修复与稳定性改进
- 新功能与新系统
- 文档与教程
- 编辑器改进与工具
- 性能优化
- 架构建议
- 提问与讨论

**任何对引擎有益的 pull request 都会被认真考虑并合并。** 我们在这里共同学习，一起打造了不起的东西。


> [!CAUTION]
> 包含 AI 生成内容的代码或文档将不被接受，我们不需要低投入的产出。

---

## 入门

### 1. 加入社区

在贡献之前，我们建议加入我们的 [Discord 服务器](https://discord.gg/xQSB7CRzQE)，以便：
- 与团队讨论你的想法
- 获得环境搭建和开发方面的帮助
- 在较大的功能上协同工作
- 结识其他贡献者

### 2. 配置开发环境

按照主 README 中的[快速开始指南](README.md#快速开始)在你的机器上完成 Lumina 的构建。

### 3. 找到可做的事

- 查看 [Issues](https://github.com/mrdrelliot/lumina/issues) 页面上的待办任务
- 寻找标记为 `good first issue` 或 `help wanted` 的 issue
- 先开一个 issue 来提出新功能
- 修复你在使用引擎时遇到的 bug

### 4. Fork 与创建分支

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR_USERNAME/luminaengine.git
cd lumina
git remote add upstream https://github.com/mrdrelliot/luminaengine.git

# Create a feature branch
git checkout -b feature/my-awesome-feature
```

---

## 编码规范

Lumina 执行严格的编码规范，以保持整个代码库的一致性与可读性。

### 命名约定

#### 前缀

所有类型都必须根据其用途使用合适的前缀：

| 前缀 | 用途 | 示例 |
|--------|-------|---------|
| `F` | 引擎内部类型（非反射） | `FRenderer`、`FTexture`、`FRenderGraph` |
| `C` | 反射类（面向游戏逻辑） | `CTransform`、`CMeshRenderer`、`CLight` |
| `S` | 反射结构体（数据类型） | `SVertex`、`SMaterial`、`SRenderSettings` |
| `E` | 枚举 | `ETextureFormat`、`ERenderPass` |
| `I` | 接口（抽象类） | `ISystem`、`IRenderable` |
| `T` | 模板参数 | `TObjectHandle<T>`、`TArray<T>` |

#### 大小写约定

```cpp
// CORRECT: PascalCase for everything
class FRenderer { };
void UpdateTransform();
int32 EntityCount;
glm::vec3 Position;
ETextureFormat Format;

// INCORRECT: No snake_case or camelCase
void update_transform();    // Wrong
int entity_count;          // Wrong
glm::vec3 m_position;        // Wrong (no Hungarian notation)
```

**所有标识符均使用 PascalCase：**
- 类、结构体、枚举
- 函数与方法
- 变量（局部、成员、全局）
- 常量
- 命名空间

### 代码风格

#### 缩进与格式

```cpp
// CORRECT: Tabs for indentation, braces on new lines
void ProcessEntities()
{
	for (const auto& Entity : Entities)
	{
		if (Entity.IsValid())
		{
			Entity.Update(DeltaTime);
		}
	}
}

// INCORRECT: Spaces and wrong brace placement
void ProcessEntities() {
    for (const auto& Entity : Entities) {
        if (Entity.IsValid()) {
            Entity.Update(DeltaTime);
        }
    }
}
```

**规则：**
- 使用 **tab** 缩进（不要用空格）
- 左花括号放在**新行**
- 每行一条语句
- 控制流关键字后加空格：`if (`、`for (`、`while (`

#### 函数设计

函数应当**职责单一、参数精简、封装良好**：

```cpp
// CORRECT: Single responsibility, minimal parameters
void UpdateEntityTransform(TObjectHandle<CTransform> Transform, const glm::vec3& NewPosition)
{
	if (!Transform.IsValid())
	{
		return;
	}
	
	Transform->SetPosition(NewPosition);
	Transform->MarkDirty();
}

// INCORRECT: Too much responsibility, unclear purpose
void DoStuff(CEntity* Entity, float X, float Y, float Z, bool Flag1, bool Flag2, int Mode)
{
	Entity->Transform->Position.X = X;
	Entity->Transform->Position.Y = Y;
	Entity->Transform->Position.Z = Z;
	
	if (Flag1)
	{
		Entity->DoSomething();
	}
	
	if (Flag2)
	{
		Entity->DoSomethingElse();
	}
	
	// ... 50 more lines
}
```

**原则：**
- **单一职责** - 一个函数只做一件事
- **参数精简** - 尽量不超过 3 个参数
- **函数短小** - 尽可能把函数控制在 50 行以内
- **尽早返回** - 先校验输入，失败时尽早返回
- **无副作用** - 函数只做其名字所暗示的事，不多做

### 内存管理

#### 句柄系统（强制要求）

**禁止使用指向 `CObject` 派生类型的裸指针。** 始终使用 `TObjectHandle<T>`：

```cpp
// CORRECT: Use TObjectHandle for safe weak references
class CMyComponent
{
public:
	void SetTarget(TObjectHandle<CEntity> InTarget)
	{
		Target = InTarget;
	}
	
	void Update()
	{
		if (Target.IsValid())
		{
			// Safe to use, handle checks validity and generation
			Target->DoSomething();
		}
	}
	
private:
	TObjectHandle<CEntity> Target;
};

// INCORRECT: Raw pointers are unsafe
class CMyComponent
{
public:
	void SetTarget(CEntity* InTarget)  // PROHIBITED!
	{
		Target = InTarget;  // Dangling pointer risk!
	}
	
private:
	CEntity* Target;  // NEVER DO THIS!
};
```

**为什么用 TObjectHandle？**
- 分代（generational）安全性可防止 use-after-free
- 对象销毁时自动失效
- 弱引用语义
- 便于序列化
- 线程安全的校验

**例外：**
以下情况允许使用裸指针：
- 非 `CObject` 类型（例如 `FRenderer*`、`FTexture*`）
- 函数的局部参数（生命周期由调用者保证）
- 平台/外部 API

### 面向数据的设计

**优先使用面向数据的编程，而非沉重的 OOP 继承。**

```cpp
// CORRECT: Data-oriented, cache-friendly
struct STransformData
{
	glm::vec3 Position;
	glm::quat Rotation;
	glm::vec3 Scale;
};

// Process in batches
void UpdateTransforms(TArray<STransformData>& Transforms, float DeltaTime)
{
	for (auto& Transform : Transforms)
	{
		// Efficient iteration, good cache locality
		Transform.Position += Transform.Velocity * DeltaTime;
	}
}

// AVOID: Heavy inheritance, poor cache performance
class CBaseTransform { virtual void Update() = 0; };
class CDerivedTransform1 : public CBaseTransform { /* ... */ };
class CDerivedTransform2 : public CBaseTransform { /* ... */ };
// Virtual calls, scattered memory, cache misses
```

**原则：**
- 将**数据与行为分离**
- 使用**普通结构体**存储数据
- 尽可能**批量**处理数据
- 在热路径中尽量减少**虚函数调用**
- 优先**组合而非继承**

### 元编程

**允许使用模板元编程，但必须用 C++20 concept 加以约束。**

```cpp
// CORRECT: Concepts enforce constraints at compile-time
template<typename T>
concept Transformable = requires(T Object)
{
	{ Object.GetPosition() } -> std::convertible_to<glm::vec3>;
	{ Object.SetPosition(glm::vec3{}) } -> std::same_as<void>;
};

template<Transformable T>
void ApplyTranslation(T& Object, const glm::vec3& Translation)
{
	Object.SetPosition(Object.GetPosition() + Translation);
}

// INCORRECT: Unconstrained template
template<typename T>
void ApplyTranslation(T& Object, const glm::vec3& Translation)
{
	// What if T doesn't have GetPosition/SetPosition?
	Object.SetPosition(Object.GetPosition() + Translation);
}
```

**要求：**
- **所有模板**都必须使用 concept 或 SFINAE
- 通过 concept 约束提供**清晰的错误信息**
- 在注释中**说明需求**
- 简单场景**避免**使用模板元编程

### 文档

**所有代码都必须有清晰、简洁的注释。**

```cpp
/**
 * Renders a mesh with the specified material and transform.
 * 
 * @param Mesh - The mesh to render (must be loaded and valid)
 * @param Material - Material to apply during rendering
 * @param Transform - World-space transformation matrix
 * 
 * @return True if rendering was successful, false if validation failed
 * 
 * @note This function is not thread-safe and must be called from the render thread
 */
bool RenderMesh(const FMesh& Mesh, const FMaterial& Material, const FMatrix& Transform);

// Comment complex logic
void CalculateClusterBounds()
{
	// Use conservative depth bounds to minimize false positives
	// Based on "Practical Clustered Shading" (Olsson et al.)
	for (uint32 Z = 0; Z < ClusterDepth; ++Z)
	{
		// Exponential depth slicing for better near-plane resolution
		float NearPlane = CalculateDepthSlice(Z);
		// ...
	}
}

// Explain why, not what
// Flush commands here to avoid synchronization issues with async compute
CommandBuffer.Submit();

// Useless comment
// Increment i by 1
++i;
```

**文档准则：**
- 所有公开 API 使用 **Doxygen 风格注释**
- 解释**为什么**这样做，而不只是代码做了什么
- 对非显而易见的算法**引用论文/文章**
- 记录**线程安全性**与**性能特征**
- 为复杂系统提供**用法示例**

---

## 测试与质量

### 代码质量检查清单

提交 PR 之前，请确认：

- [ ] 代码在 MSVC 上编译无警告
- [ ] 遵循所有命名与风格约定
- [ ] 没有裸 `CObject*` 指针（使用 `TObjectHandle`）
- [ ] 函数短小且职责单一
- [ ] 所有模板都使用了 concept
- [ ] 代码文档完善
- [ ] 无内存泄漏或崩溃
- [ ] 已考虑性能影响


**测试要求：**
- 为新功能编写测试
- 确保现有测试通过
- 测试边界情况和错误条件
- 确认没有性能退化

---

## Pull Request 流程

### 1. 提交之前

- 先在 Discord 或 issue 中讨论重大改动
- 确保代码遵循上述所有规范
- 按需更新文档
- 充分测试
- 变基（rebase）到最新的 `main` 分支

### 2. 提交信息

编写清晰、有描述性的提交信息：

```bash
# GOOD. Clear, specific, imperative mood
Add clustered lighting support to deferred renderer
Fix crash when destroying entities during iteration
Optimize mesh batching for static geometry

# BAD: Vague, unclear, or too casual
Fixed stuff
WIP
Updated things
asdfasdf
```

**格式：**
```
[Category] Brief description (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain the problem this commit solves and why you chose
this particular solution.

Fixes #123
```

**类别：** `Feature`、`Fix`、`Refactor`、`Docs`、`Test`、`Perf`

### 3. Pull Request 模板

发起 PR 时，请包含：

```markdown
## Description
Brief overview of what this PR does.

## Motivation
Why is this change needed? What problem does it solve?

## Changes Made
- Added X system
- Refactored Y for better performance
- Fixed crash in Z

## Testing
- [ ] Tested in Editor
- [ ] Tested in Sandbox
- [ ] No performance regression
- [ ] All existing tests pass

## Screenshots (if applicable)
[Add screenshots for visual changes]

## Related Issues
Fixes #123
Related to #456
```

### 4. 评审流程

- 维护者会在几天内评审你的 PR
- 处理反馈和所要求的修改
- 一经批准，你的 PR 就会被合并！
- 你会在发行说明中获得署名

---

## 🎯 优先领域

我们尤其欢迎贡献的领域：

### 高优先级
- **性能优化** - 性能分析与瓶颈修复
- **平台支持** - Linux/macOS 移植
- **文档** - API 文档、教程、示例
- **Bug 修复** - 稳定性改进

### 中优先级
- **编辑器功能** - 工作流程改进
- **渲染功能** - 新技术与效果
- **资源管线** - 导入/导出改进
- **脚本** - 游戏逻辑脚本支持

### 锦上添花
- **示例项目** - 演示与样例
- **工具** - 外部编辑器插件
- **CI/CD** - 自动化测试与构建

---

## 资源

### 学习资料
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Game Engine Architecture](https://www.gameenginebook.com/)
- [Real-Time Rendering](https://www.realtimerendering.com/)
- [GPU Gems](https://developer.nvidia.com/gpugems/gpugems/contributors)

### Lumina 架构
- [引擎概览](docs/EngineOverview.md) — 仓库布局、模块与 target、启动流程、主循环
- [渲染架构](docs/RenderingArchitecture.md) — RHI、shader、场景渲染的 pass 顺序
- [ECS 指南](docs/ECSGuide.md) — `FRegistry`、组件存储、typed view
- [反射系统](docs/ReflectionSystem.md) — 标记宏、Reflector、序列化与 C# 绑定

以上文档的入口与阅读约定见 [docs/README.md](docs/README.md)。

---

## 有问题？

**尽管提问！** 我们在这里提供帮助：

- [Discord 服务器](https://discord.gg/xQSB7CRzQE) - 实时聊天
- [GitHub Issues](https://github.com/mrdrelliot/lumina/issues) - Bug 报告与功能请求
- 敏感事务请直接联系维护者

---

## 贡献者

感谢我们所有的贡献者！每一份贡献，无论大小，都让 Lumina 变得更好。

<!-- ALL-CONTRIBUTORS-LIST:START -->
<!-- This section will be automatically updated -->
<!-- ALL-CONTRIBUTORS-LIST:END -->

---

## 行为准则

### 我们的承诺

我们致力于为每个人提供一个友好、包容的环境，无论：
- 经验水平
- 性别认同与表达
- 性取向
- 残障情况
- 外貌
- 体型
- 种族或民族
- 年龄
- 宗教
- 国籍

### 我们的标准

**值得提倡的行为包括：**
- 使用友好、包容的语言
- 尊重不同的观点
- 优雅地接受建设性批评
- 以社区利益为重
- 对他人展现共情

**不可接受的行为包括：**
- 骚扰、挑衅或侮辱性言论
- 人身或政治攻击
- 公开他人隐私信息
- 任何在职业场合不恰当的行为

### 执行

违规行为可能导致被临时或永久禁止参与本项目。请通过 Discord 或电子邮件向项目维护者报告问题。

---

<div align="center">

## 谢谢！

**你的贡献让 Lumina 得以存在。**

每一个 bug 修复、每一项功能和每一处文档改进，都帮助整个社区学习与成长。

**让我们一起打造了不起的东西！**

[开始贡献 →](https://github.com/mrdrelliot/lumina/issues)

</div>
