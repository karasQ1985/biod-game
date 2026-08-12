# Biod 参数调节器 (Param Tuner)

可视化修改项目中硬编码的粒子上限、植被数量等参数，无需手动编辑源文件。支持一键编译运行闭环。

## 启动

```powershell
cd biod-game
python tools/param_tuner.py
```

依赖：Python 3.7+（仅需标准库 tkinter）。

## 功能

| 功能 | 说明 |
|------|------|
| 参数分组 | 全局粒子上限 / 单群体上限 / 群体种类 / 植物数量，共 12 个参数 |
| 中文引导 | 每个参数附带中文标签和说明文字，悬停可见详细描述 |
| 滑块 + 输入 | 拖拽滑块或直接输入数值，实时高亮偏离默认值的参数（橙色） |
| 复位按钮 | 单个参数 ↺ 复位 / 整组复位 / 底部「复位全部」一键恢复默认值 |
| 应用修改 | ✔ 按钮将所有参数写回源文件，首次修改自动创建 .bak 备份 |
| 编译并运行 | ▶ 按钮：应用 → 调用 cmake --build 编译 → 成功后可按 ▷ 运行 |
| 构建日志 | 终端风格暗色面板，实时流式显示编译输出 |

## 管理的参数一览

### 全局粒子上限（4 项，必须同值）

| 参数 | 文件 | 默认值 |
|------|------|--------|
| Simulation 初始化上限 (m_maxBoids) | `src/ui/GLWidget.cpp` | 10000 |
| Renderer GPU VBO 缓冲区 | `src/renderer/Renderer.cpp` | 10000 |
| Renderer CPU 实例缓冲区 | `src/renderer/Renderer.cpp` | 10000 |
| UI 全局上限滑块 | `src/ui/MainWindow.cpp` | 10000 |

### 单群体粒子上限（3 项）

| 参数 | 文件 | 默认值 |
|------|------|--------|
| ReproductionParams::maxFlockSize 默认值 | `src/simulation/FlockData.h` | 2000 |
| setGlobalFlockCap() 硬钳制值 | `src/simulation/Simulation.cpp` | 2000 |
| 单群体上限滑块最大值 | `src/ui/MainWindow.cpp` | 5000 |

### 群体种类上限

| 参数 | 文件 | 默认值 |
|------|------|--------|
| MAX_FLOCKS 最大群体种类数 | `src/simulation/Simulation.h` | 12 |

### 植物数量上限（3 项）

| 参数 | 文件 | 默认值 |
|------|------|--------|
| PlantParams::maxPlants 默认值 | `src/simulation/PlantData.h` | 200 |
| 植物滑块最大值 | `src/ui/MainWindow.cpp` | 500 |
| 初始植物数量 | `src/simulation/PlantData.h` | 80 |

## 安全保障

- 首次应用任何文件前自动创建 `.bak` 备份（与源文件同目录）
- 同步组内参数值不一致时弹窗阻止应用
- 修改后必须编译才生效；工具未运行时源文件不受影响
- 关闭工具不会自动保存——只有点击「应用修改」或「编译并运行」才写入文件

## 恢复原始文件

如需完全恢复源文件到 git 版本：

```powershell
git checkout -- src/
```

或手动复制 `.bak` 文件覆盖对应源文件（应用修改时自动生成）。
