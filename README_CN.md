# Biod - 群体行为模拟器

基于 Reynolds 群体行为模型的实时二维 Boid 模拟器，使用 C++17 / Qt6 / OpenGL 3.3 构建。

## 已实现功能

### 核心模拟
- **Reynolds 三规则**：分离 (Separation)、对齐 (Alignment)、聚集 (Cohesion)，各自独立权重
- **硬碰撞系统**：近距离排斥力防止个体堆叠，碰撞距离可调 (0-100 px)
- **边界规避**：软排斥力将个体推回世界内部，边缘处环面穿越作为兜底
- **随机游荡**：每帧随机方向推力，增加自然的有机波动
- **空间哈希网格**：O(n) 邻居搜索，单元格尺寸自适应聚集半径

### 多群体系统
- 动态添加/删除群体（全局上限 10000 个体，单群上限 2000）
- **捕食者-猎物关系**：定向追捕、恐惧逃逸、连杀增重系统
- 中性群间斥力、自定义群体名称和颜色
- 雄性/雌性分别着色（支持性别颜色开关）

### 饥饿与体重系统
- 饥饿值持续衰减，低于阈值触发觅食、闪烁警告
- 饱腹/饥饿状态下速度动态调节（可反转曲线）
- 连杀增重 (Kill Streak)：连续猎杀积累体重，闲置后衰减
- 体重影响移动速度和渲染大小

### 植物生态
- 植物随机生长、扩散、季节性变化
- 施肥机制：捕食者击杀产生的尸体促进周围植物生长
- **PlantSpatialHash**：植物空间哈希优化觅食/放牧/施肥 O(boids×plants)→O(boids×nearby)

### 繁殖系统
- 基于性别的配对繁殖（雄-雌），每群独立参数控制
- 随机后代数量、繁殖间隔、最低饱腹度要求、群体容量上限

### 渲染
- **单次 Draw Call**：`glDrawElementsInstanced` 实例化绘制
- **精灵图系统**：`GL_TEXTURE_2D_ARRAY` 纹理数组，每群独立精灵图
- 精灵图正立模式：移动方向变化时图像始终保持 Y 轴向上
- 精灵图缩放滑块 (2x-100x)，饥饿闪烁效果
- 视图缩放：鼠标滚轮以鼠标位置为中心缩放 (0.1x-10x)

### UI
- **集中式参数注册表 (ParamRegistry)**：一行代码注册新参数滑块
- 自动步进：根据参数范围自动推导合理的键盘步长
- 持久数值标签：每个滑块右侧实时显示当前值
- 右键手动输入：右键滑块弹出精确数值输入对话框
- 中英双语滑块标签

## 架构

```
src/
  main.cpp                    入口 (QApplication + OpenGL 上下文)
  core/
    ParamDef.h                参数定义、ScaleMode、滑块-浮点转换
    ParamRegistry.h/.cpp      集中式参数注册表
  simulation/
    FlockData.h               SoA 数据布局, FlockParams
    PlantData.h               植物 SoA 数据
    SpatialHash.h/.cpp        Boid 空间哈希网格
    PlantSpatialHash.h/.cpp   植物空间哈希网格
    Simulation.h/.cpp         物理更新、Reynolds 规则、繁殖、捕猎
  renderer/
    Renderer.h/.cpp           实例化渲染、精灵图纹理管理
  ui/
    GLWidget.h/.cpp           OpenGL 画布、鼠标交互
    MainWindow.h/.cpp         工具栏、参数面板、群体管理
```

### 关键设计决策
- **SoA 布局**：位置、速度、颜色分离存储，提升 CPU 缓存命中率
- **零每帧堆分配**：所有缓冲区 `reserve()` + `clear()`，永不 `new`/`delete`
- **单次遍历更新**：所有行为规则在同一个个体循环中完成
- **单次 Draw Call**：实例化渲染，与个体数量无关

## 构建

**依赖**：CMake 3.16+, Qt 6 (Widgets + OpenGLWidgets), OpenGL 3.3+

```bash
mkdir build && cd build
cmake .. -G Ninja
cmake --build .
```

Windows 部署：
```bash
windeployqt Biod.exe
# 手动复制 MinGW 运行时 DLL: libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll
```

精灵图图片需放置在可执行文件同级的 `image/` 目录下（64x64 透明 PNG）。

## 性能

测试环境：Windows 10, Intel 集成显卡

| 场景 | 个体数 | FPS | 物理耗时 | 渲染耗时 |
|------|-------|-----|---------|---------|
| 基础单群 | 500 | ~200 | ~1.3ms | ~0.5ms |
| 基础单群 | 3000 | ~115-184 | ~2-6ms | ~1.5ms |
| 基础单群 | 5000 | ~80-130 | ~5-11ms | ~2.5ms |
| 多群体 (3x500) | 1500 | ~150-200 | ~2-4ms | ~1.0ms |

## 已知局限

1. 无障碍物规避：个体只对世界边界有反应
2. 性能日志同步写入：每秒短暂阻塞 UI 线程
3. ParamDef 仅支持 float 字段：非 float 参数需手动扩展

## 后续规划

- [ ] 静态/动态障碍物规避
- [ ] 万级以上个体计算着色器卸载
- [ ] 保存/加载群体配置
- [ ] 模拟录制与回放
