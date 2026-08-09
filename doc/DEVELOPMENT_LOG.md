# Biod - 群体行为模拟器 开发日志

> 最后更新: 2026-08-10
> 项目: 实时二维群体（Boid）模拟器 (C++17 / Qt6 / OpenGL 3.3)

---

## 1. 架构概览

```
src/
  main.cpp                    入口 (QApplication + OpenGL 上下文)
  core/
    ParamDef.h               参数定义、ScaleMode、滑块-浮点转换辅助
    ParamRegistry.h/.cpp     集中式参数注册表 (slider 构建、回调连接、刷新)
  simulation/
    FlockData.h               SoA 数据布局 (Structure of Arrays), FlockParams, FlockData
    PlantData.h               植物数据 (SoA)
    SpatialHash.h/.cpp        空间哈希网格 (Spatial Hashing) 实现 O(n) 邻居搜索
    PlantSpatialHash.h/.cpp   植物空间哈希网格 (觅食/放牧/施肥优化)
    Simulation.h/.cpp         Reynolds 三规则 + 边界规避 + 随机游荡 + 多群体
  renderer/
    Renderer.h/.cpp           单次实例化绘制 (glDrawElementsInstanced), 每个体独立颜色
  ui/
    GLWidget.h/.cpp           OpenGL 画布, 鼠标交互, 帧计时
    MainWindow.h/.cpp         工具栏, 参数滑块, 群体管理
```

### 关键设计决策

- **SoA (Structure of Arrays，数组结构体布局)**：位置 X/Y、速度 X/Y 分别存储在独立的连续数组中，提升 CPU 缓存命中率。
- **零每帧堆内存分配（Zero per-frame heap allocation）**：所有工作缓冲区 (`forceX/Y`, `neighbors`) 在初始化时通过 `resize()` / `reserve()` 预分配，每帧只做 `clear()` / `fill()`，永不 `new`/`delete`。
- **单次遍历更新循环（Single-pass update loop）**：分离、对齐、聚集、硬碰撞、边界规避、随机游荡、目标跟随全部在同一个个体循环中完成。
- **单次绘制调用（Single draw call）**：`glDrawElementsInstanced` 一次性渲染所有个体，与个体数量无关。
- **空间哈希网格（Spatial hash grid）**：单元格尺寸 = cohesionRadius × 1.5；每个体查询 3×3 邻域；时间复杂度 O(n)。

---

## 2. 已修复的 Bug

### 2.1 启动崩溃: updateFlockButtons() 中的空指针解引用

- **日期**：2026-08-09
- **现象**：应用双击启动后立即崩溃，无任何错误输出。
- **根因**：`MainWindow` 构造函数中先调用了 `setupToolbar()`，再调用 `setupUI()`。`setupToolbar()` 内部又调用了 `updateFlockButtons()`，该方法中使用了 `m_flockLabel->setText(...)`，但 `m_flockLabel` 是在 `setupUI()` 中才被创建的，此时为 `nullptr`（空指针），解引用导致崩溃。
- **修复**：将 `setupUI()` 的调用调整到 `setupToolbar()` 之前。同时在 `updateFlockButtons()` 内部增加了 `if (m_flockLabel)` 空指针保护作为防御性编程（defense-in-depth）。
- **涉及文件**：[src/ui/MainWindow.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\ui\MainWindow.cpp)

### 2.2 群体塌缩为一个点

- **日期**：2026-08-09
- **现象**：当聚集力权重（cohesionWeight）非零时，所有个体会快速聚拢成一个紧密的点，之后永远不再散开。参数滑块的调节几乎看不到效果。
- **根因分析**：

  1. **力的归一化（Normalization）破坏了距离敏感性**。分离力被归一化到 `maxSpeed` 量级，失去了距离信息。在对称集群中，来自各个方向的邻居产生的分离向量相互抵消——归一化后的净分离力是随机方向，并不能将个体推出集群。

  2. **力的大小量纲不统一**。原始分离力（反距离加权：`1/distSq`）在近距离时可达数万，而对齐力/聚集力的 steering 值仅在 0~600 之间。三者混合后被 `maxForce=600` 截断（clamp），相对差异完全丢失，滑块调节失效。

- **修复**：将力分为两个层级：

| 层级 | 距离范围 | 力的类型 | 归一化 |
|------|---------|---------|--------|
| 硬碰撞（Hard collision） | < boidSize × 2.5 (15px) | 二次交叠力 | 不归一化 |
| 软分离（Soft separation） | < separationRadius (30px) | 反距离加权 `1/distSq` | 不归一化，仅上限截断 |
| 对齐（Alignment）/ 聚集（Cohesion） | < 各自感知半径 | 标准 Steering | 归一化到 maxSpeed |

  - 分离力**永不归一化**——保留天然的距离衰减特性。
  - 硬碰撞半径（`boidSize × 2.5`）施加二次交叠力，保证可以克服任何聚集力。
  - `maxForce` 从 600 提升到 3000，适配归一化重构后更宽的力的范围。

- **涉及文件**：[src/simulation/Simulation.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\Simulation.cpp), [src/simulation/FlockData.h](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\FlockData.h)

### 2.4 OpenGL 上下文缺失导致纹理操作静默失败

- **日期**：2026-08-09
- **现象**：Import 精灵图后图片无法显示；重新加载后已有纹理被破坏。程序运行中无报错。
- **根因**：`initSprites()` 在 `QTimer::singleShot(100ms)` 回调中调用了 `renderer.loadSprites()`，内部执行 `glDeleteTextures`、`glGenTextures`、`glTexImage3D` 等 GL 操作。此时不在 `paintGL()`/`initializeGL()` 调用栈中，**GL 上下文不是 current**——所有 GL 调用静默失败：旧纹理被无效删除，新纹理无法创建。
- **修复**：在 GL 操作前调用 `m_glWidget->makeCurrent()` 激活上下文，操作后调用 `doneCurrent()` 释放。
- **教训**：任何不在 OpenGL Widget 的回调（paintGL/initializeGL/resizeGL）中执行的 GL 操作，必须手动 makeCurrent。否则行为不可预测——失败是静默的，没有 OpenGL 错误码返回。
- **涉及文件**：[src/ui/MainWindow.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\ui\MainWindow.cpp)

### 2.5 文件自删除陷阱（Self-Delete Trap）

- **日期**：2026-08-09
- **现象**：使用 Import 按钮将已存在于 `image/` 目录的图片"重新导入"时，该图片文件被永久删除。
- **根因**：`onImportSprite()` 逻辑为：先 `QFile::remove(destPath)` 删除可能存在的旧文件，再 `QFile::copy(filePath, destPath)` 复制新文件。当用户在文件对话框中选择的是 `image/` 目录下已存在的文件（即 `filePath == destPath`），`remove` 删除了源文件本身，后续 `copy` 必然失败。
- **修复**：复制前通过 `QFileInfo::canonicalFilePath()` 比较源和目标是否为同一文件。若相同，跳过 `remove`+`copy` 步骤，直接 reload。
- **教训**：任何"先删后写"的复制逻辑，必须先验证 source ≠ destination。在 Windows 上直接比较字符串路径不可靠（大小写、斜杠方向、短路径 vs 长路径），应使用 `canonicalFilePath()`。
- **涉及文件**：[src/ui/MainWindow.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\ui\MainWindow.cpp)

### 2.6 GLSL Attribute Location 与 CPU 端不同步

- **日期**：2026-08-09
- **现象**：添加 `aUpright` 实例属性后，精灵图完全不显示（黑色/透明），无论缩放倍率如何。
- **根因**：Vertex Shader 中新增 `layout(location = 6) in float aUpright;`，导致原 `aUV` 从 location 6 变为 location 7。但 CPU 端 `init()` 中 `glVertexAttribPointer(6, ...)` + `glEnableVertexAttribArray(6)` 指向 `aUV` 的调用仍使用旧 location 6——此时 location 6 已被 `aUpright` 占据，`aUV` 收到的是 `upright` 的 float 值而非 vec2 UV 坐标，纹理采样失败。
- **修复**：将 CPU 端的 `glVertexAttribPointer(6, ...)` 同步更新为 `glVertexAttribPointer(7, ...)`。
- **教训**：Shader 中任何 location 的插入/移动，必须同步修改所有对应的 `glVertexAttribPointer`、`glEnableVertexAttribArray`、`glVertexAttribDivisor` 调用。建议在 `init()` 中为每个 attribute 加注释标明其对应 shader layout location，方便将来交叉验证。
- **涉及文件**：[src/renderer/Renderer.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\renderer\Renderer.cpp)

### 2.7 QImage 与 OpenGL 纹理 Y 轴方向不一致

- **日期**：2026-08-09
- **现象**：精灵图显示上下颠倒（山羊头朝下）。
- **根因**：QImage 像素数据按**从顶到底**存储（`bits()[0]` 是图像最顶行），而 OpenGL `glTexImage3D`/`glTexSubImage3D` 将第一行数据解释为纹理**底部**。两套坐标系的 Y 轴方向相反。
- **修复**：上传 GPU 前执行 `img.flipped(Qt::Vertical)` 垂直翻转。
- **涉及文件**：[src/renderer/Renderer.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\renderer\Renderer.cpp)

### 2.8 ParamRegistry 类型不匹配导致繁殖数值爆炸

- **日期**：2026-08-10
- **现象**：对某个群体设置繁殖参数（Min/Max Offspring）后切换群体，被设置的群体会**瞬时产生海量后代**，程序 CPU 占满进入假死状态（不崩溃但不响应）。
- **根因**：`FlockParams` 中 `reproductionMinOffspring` 和 `reproductionMaxOffspring` 声明为 `int` 类型，但 `ParamDef` 的 `readFrom()`/`writeTo()` 内部使用 `reinterpret_cast<float*>` 对该字段进行读写。当滑块值为 5 时，`writeTo()` 将 5.0f 的比特模式（0x40A00000）写入 int 字段，该值作为 int 解释约为 **10.8 亿**。繁殖逻辑的内层循环使用此值控制迭代次数，导致单帧内执行数亿次条件判断，程序卡死。
- **修复**：
  1. 将 `FlockParams` 中 4 个字段从 `int` 改为 `float`：`reproductionMinOffspring`、`reproductionMaxOffspring`、`maxFlockSize`；`PlantParams` 中 `maxPlants`、`initialPlants`。
  2. 在使用处添加 `static_cast<int>()` 显式转换（循环边界、数组索引等位置）。
- **教训**：`ParamDef` 通过 `reinterpret_cast<float*>` 统一读写所有参数，**隐含假设所有被注册的字段都是 `float` 类型**。注册非 float 字段会在运行时产生比特级别的数据污染——编译器不会报警、静态分析不会发现，只有运行时症状（数值异常大/小/NaN）。解决方案有两种：(a) 统一所有可调参数为 float，在使用处 cast —— 本期采用；(b) 在 `ParamDef` 中引入类型标记，分支处理不同类型 —— 后续可考虑。
- **涉及文件**：[src/simulation/FlockData.h](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\FlockData.h), [src/simulation/PlantData.h](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\PlantData.h), [src/simulation/Simulation.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\Simulation.cpp), [src/ui/MainWindow.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\ui\MainWindow.cpp)

---

### 2.3 运行时找不到 Qt6 DLL

- **日期**：2026-08-09
- **现象**：直接双击 `Biod.exe` 无法运行——缺少 Qt6 相关 DLL 和 MinGW 运行时 DLL。
- **修复**：
  1. 运行 `windeployqt` 自动部署 Qt6 Core/Gui/Widgets DLL、平台插件（platform plugin）和图像格式插件。
  2. 手动从 Qt 的 MinGW 工具链目录复制运行时 DLL：`libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` 到构建输出目录。
- **备注**：`windeployqt` 不会自动检测 MinGW 运行时依赖。

---

## 3. 功能演进记录

### 3.1 GPU 性能规范中增加 CPU 开销要求

- **日期**：2026-08-09
- 在 `GPU_PERFORMANCE_RULES.md` 中新增第 0 节（最高优先级）：
  - 0.1：零每帧堆内存分配（禁止 `new`/`delete`/临时容器）
  - 0.2：热路径禁止虚函数（virtual function）；禁止循环内多层小函数调用
  - 0.3：强制 SoA 数据布局（FlockData 分离存储 posX/Y、velX/Y）
  - 0.4：预分配模式（`reserve()` + `clear()`）
- 新增第 4 节（禁止事项清单）。

### 3.2 滑块标签中英双语化

- **日期**：2026-08-09
- 所有参数滑块标签改为 `中文名称 (English Name)` 格式。
- 映射表：
  - `Separation` → `分离 (Separation)`
  - `Alignment` → `对齐 (Alignment)`
  - `Cohesion` → `聚集 (Cohesion)`
  - `Boundary Avoid` → `边界规避 (Boundary Avoid)`
  - `Boundary Margin` → `边界距离 (Boundary Margin)`
  - `Wander` → `随机游荡 (Wander)`
  - `Sep Radius` → `分离半径 (Sep Radius)`
  - `Ali Radius` → `对齐半径 (Ali Radius)`
  - `Coh Radius` → `聚集半径 (Coh Radius)`
  - `Inter-Flock Repulsion` → `群间斥力 (Inter-Flock Repulsion)`

### 3.3 边界规避（Boundary Avoidance）

- **日期**：2026-08-09
- 当个体接近世界边界 `boundaryMargin`（默认 80px）范围内时，施加一个指向世界内部的软排斥力。
- 力的大小与边界距离成线性反比（margin 处为 0，边缘处为 1.0）。
- 环面穿越（toroidal wrapping）保留作为兜底——如果个体因其他力被推出边界，仍会从对侧出现。

### 3.4 随机游荡（Random Wander）

- **日期**：2026-08-09
- 每个体每帧获得一个随机方向的小幅度推力。
- 随机角度通过 `std::mt19937` + `std::uniform_real_distribution` 均匀分布。
- 增加有机的自然波动，防止群体行为过于整齐划一。

### 3.5 感知半径滑块

- **日期**：2026-08-09
- 新增三个滑块：分离半径、对齐半径、聚集半径。
- 安全边界（Safety bounds）：分离 5-100，对齐 10-200，聚集 10-200。
- 修改聚集半径时自动触发 `SpatialHash::reinit()` 重建网格（网格单元尺寸 = cohesionRadius × 1.5）。
- 新增 `Simulation::updateGrid()` 和 `SpatialHash::reinit()` 方法。

### 3.6 多群体系统（Multi-Flock）

- **日期**：2026-08-09
- **数据层**：`FlockData` 新增 `flockId[]`、`colorR[]`、`colorG[]`、`colorB[]`（SoA 布局）。
- **行为规则**：
  - 同群（Same-flock）：执行完整 Reynolds 三规则（分离 + 对齐 + 聚集）。
  - 异群（Different-flock）：仅施加群间斥力——反距离衰减，距离越近力越强。
- **可视化**：GLSL 着色器（shader）中新增每个体颜色属性（location 3）。四组预设颜色：红（A）、蓝（B）、绿（C）、金（D）。
- **UI**：工具栏新增四个彩色群体切换按钮，选中群体显示白色边框高亮。参数面板新增「群间斥力」滑块（0.0-5.0，默认 1.5）。
- **实现细节**：
  - `FlockParams::interFlockRepulsionWeight` 控制斥力强度。
  - 群间斥力的作用半径与聚集半径（cohesionRadius）一致。
  - 空间哈希网格在所有群体间共享（无需每群独立网格）。
- **涉及文件**：[FlockData.h](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\FlockData.h), [Simulation.h/.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\Simulation.h), [Renderer.h/.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\renderer\Renderer.h), [MainWindow.h/.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\ui\MainWindow.h)

### 3.7 PlantSpatialHash 植物空间哈希优化

- **日期**：2026-08-10
- **背景**：模拟运行一段时间后出现严重卡顿。分析发现三个热点循环均为 O(boids × plants) 全扫描：Foraging（觅食）、Grazing（进食）、Fertilization（施肥）。即使植物数量仅 80 个，单帧迭代量已达 80 × boidCount，累积效应明显。
- **方案**：新增 `PlantSpatialHash` 类，与已有的 boid 空间哈希平行运作。植物每帧 `rebuild()`，查询时只检查 3×3 邻域细胞块。
- **效果**：预估 6-10 倍加速。Foraging 循环从 ~200K 迭代/帧降至 ~15K 迭代/帧。
- **关键实现细节**：
  - `rebuild()` 跳过 growth <= 0 的死亡植物
  - `queryNeighbors()` 返回邻域内植物索引列表（引用参数，零堆分配）
  - 与 boid 空间哈希共享同一设计模式：`init(世界尺寸, 细胞尺寸, 最大数量)` → `rebuild(数据)` → `queryNeighbors(x, y, 半径, 数据, 输出)`
- **涉及文件**：[src/simulation/PlantSpatialHash.h](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\PlantSpatialHash.h), [src/simulation/PlantSpatialHash.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\PlantSpatialHash.cpp), [src/simulation/Simulation.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\simulation\Simulation.cpp)

### 3.8 滑块 UX 全面改进

- **日期**：2026-08-10
- **问题**：原有滑块系统存在四项不足：(1) 精确度不一致——Div100/Div1000/OneToOne 混合导致小数位数不统一；(2) 无规范步进——所有滑块步长均为 1，范围 50-5000 的滑块一次只能移动 1 单位；(3) 无持久数值显示——必须鼠标悬停才看到 tooltip；(4) 无手动输入接口。
- **改进**：
  - **自动步进**：`ParamDef::autoSingleStep()` 根据范围自动选择步长（>3000→50, >500→10, >150→5, 其余→1），同时设置 `pageStep` 为范围的 1/10
  - **持久数值标签**：每行布局改为 `[参数名 | stretch | 加粗数值]`，滑块拖动时实时刷新，切换群体时由 `refresh()` 同步更新
  - **右键手动输入**：滑块设置 `CustomContextMenu`，右键弹出 `QInputDialog::getDouble()`，范围自动限制，小数位数与 ScaleMode 匹配
  - **精确度统一**：新增 `ParamDef::displayDecimals()`，Div1000→3 位，Div100→2 位，Div10→1 位，整数型（OneToOne/Multiply2）→0 位
- **涉及文件**：[src/core/ParamDef.h](file:///d:\HuChao_Dev\Projects\Biod\src\core\ParamDef.h), [src/core/ParamRegistry.h](file:///d:\HuChao_Dev\Projects\Biod\src\core\ParamRegistry.h), [src/core/ParamRegistry.cpp](file:///d:\HuChao_Dev\Projects\Biod\src\core\ParamRegistry.cpp)

---

## 4. 性能基准测试

所有测试均在 Windows 10 环境下运行（集成显卡 Intel iGPU）。

| 测试场景 | 个体数 | FPS | 物理耗时 (Sim) | 渲染耗时 (Render) |
|---------|-------|-----|---------------|------------------|
| 基础单群 | 500 | ~200 | ~1.3ms | ~0.5ms |
| 基础单群 | 3000 | ~115-184 | ~2-6ms | ~1.5ms |
| 基础单群 | 5000 | ~80-130 | ~5-11ms | ~2.5ms |
| 多群体（3群×500） | 1500 | ~150-200 | ~2-4ms | ~1.0ms |

所有配置均远超 30 FPS 最低目标。

---

## 5. 已知局限

1. **无障碍物规避（Obstacle Avoidance）**：个体只对世界边界有反应，不对内部障碍物规避。
2. **性能日志为同步写入**：`QFile` 的磁盘写入每秒短暂阻塞 UI 线程一次（每秒一次影响可忽略，但不够理想）。
3. **ParamDef 仅支持 float 字段**：所有注册参数必须是 float 类型（参见 2.8 教训），如需新增 int/bool 参数需扩展 ParamDef 类型标记。

---

## 6. 后续规划（Roadmap）

- [x] 每群独立参数覆盖（per-flock parameter override）
- [x] 捕食者-猎物群间关系（predator-prey relationship）
- [x] 精灵图系统（per-flock sprite textures）
- [x] 视图缩放（mouse wheel zoom）
- [ ] 静态/动态障碍物规避（static/dynamic obstacle avoidance）
- [ ] 万级以上个体时迁移到计算着色器（compute shader offload）
- [ ] 保存/加载群体配置（save/load flock configuration）
- [ ] 录制与回放模拟过程（record/replay simulation sessions）

---

## 7. 精灵图模块核心经验

### 7.1 GL 操作必须在活跃上下文中执行

Qt `QOpenGLWidget` 仅在 `initializeGL()`、`paintGL()`、`resizeGL()` 回调中自动激活 GL 上下文。从 `QTimer` 回调、按钮槽函数、文件对话框返回后调用任何 `gl*` 函数前，必须手动 `makeCurrent()`。失败是静默的——没有异常、没有错误码、没有崩溃，只有渲染结果不正确。

### 7.2 Shader Location 变更流程

当向 Vertex Shader 插入新的 `layout(location=N)` 属性时，必须同时更新：
1. CPU 端 `glVertexAttribPointer(N, ...)` 调用中的 **location 参数**
2. `glEnableVertexAttribArray(N)` 中的 location
3. `glVertexAttribDivisor(N, 1)` 中的 location
4. 所有后续 attribute 的 offset 和 stride
5. VBO 分配大小（`glBufferData` 的 size 参数）
6. CPU 端 `m_instanceData` 的填充循环（stride 常量 + 每个偏移量）

漏掉任何一项都会导致 attribute 错位——glVertexAttribPointer 将错误的数据解释为错误的类型（例如把 `float` 当 `vec2`），渲染结果难以调试。

### 7.3 文件操作安全模式

"先删后写" 模式必须包含 **同文件检测**：使用 `QFileInfo::canonicalFilePath()` 比较 source 和 dest，而不是字符串比较（Windows 上路径大小写不敏感、斜杠方向不同、短路径 vs 长路径都可能造成误判）。

### 7.4 图像坐标系桥接

QImage 和 OpenGL 纹理的 Y 轴方向相反：

| 框架 | 原点位置 | 方向 |
|------|---------|------|
| QImage `bits()` | 图像顶行 | 向下增长 |
| OpenGL `glTexImage*` | 纹理底行（first element = lower-left） | 向上增长 |

桥接方案：上传前执行 `QImage::flipped(Qt::Vertical)`。Qt 6.11 中 `mirrored()` 已废弃，推荐使用 `flipped()`。

### 7.5 精灵图正立旋转

精灵图应避免 180° 旋转（导致图像倒立）。正确的旋转策略：
- 使用 `abs(velocity.x)` 代替 `velocity.x` 计算旋转角度，将角度限制在 [-90°, +90°]
- 当 `velocity.x < 0` 时，额外施加水平镜像（`flipX = -1.0`）使精灵图面朝左
- 结果：精灵图永远正立，Y 轴始终指向世界 Y 正方向

### 7.6 纹理数组优于多纹理单元

`GL_TEXTURE_2D_ARRAY` 对比"每群体一个独立纹理"的方案优势：
- 单次 draw call（无需按群体分批绘制）
- 零排序开销（无需按纹理分组 boid）
- 仅需 1 个额外 float 实例数据（`texLayer`）
- 易于扩展：添加新精灵图只需调用一次 `glTexSubImage3D`

### 7.7 动态纹理重载的哈希策略

精灵图刷新使用 MD5 哈希对比而非文件修改时间：
- 文件修改时间不可靠——外部编辑器可能保存未修改的文件
- MD5 仅在文件内容变化时触发 GPU 纹理重建
- `std::vector<QByteArray>` 存储已加载文件的哈希，与当前文件哈希逐一比较
- 成本极低（64×64 PNG 的 MD5 耗时可忽略）
