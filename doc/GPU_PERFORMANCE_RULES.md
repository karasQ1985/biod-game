# GPU/CPU 性能优化规范

## 核心原则

在普通集成显卡上流畅运行 1000+ 个体，帧率 >= 30 FPS。所有代码必须同时考虑:

- **GPU 低消耗**: 少绘制调用、低显存
- **CPU 低开销**: 零每帧堆内存分配、最少函数调用开销、高缓存命中率
- **优先 CPU 优化**: 群体模拟的瓶颈通常在 CPU（邻居搜索、行为计算），必须先消除 CPU 端的分配和调用开销

---

## 0. CPU 开销控制（最高优先级）

### 0.1 零每帧堆内存分配

- 禁止在 `update()` / 每帧调用路径中使用 `new`、`delete`、`malloc`、`free`
- 禁止在每帧路径中使用 `std::vector::push_back`（可能触发扩容）
- 禁止在每帧路径中创建临时 `std::vector`、`std::string` 等动态分配容器
- 所有工作缓冲区必须在初始化时一次性分配，每帧复用

```cpp
// 正确: 初始化时分配，每帧复用
class Simulation {
    std::vector<int> neighborCache;   // 复用缓冲区
    std::vector<float> distCache;     // 复用缓冲区

    void init() {
        neighborCache.reserve(MAX_BOIDS);
        distCache.reserve(MAX_BOIDS);
    }

    void update() {
        neighborCache.clear();  // 仅清空，不释放内存
        // ... 使用 neighborCache
    }
};

// 错误: 每帧分配临时 vector
void update() {
    std::vector<int> neighbors;  // 每帧堆分配
    for (auto& boid : boids) {
        neighbors = findNeighbors(boid);  // 可能再次分配
    }
}
```

### 0.2 减少函数调用开销

- 热路径（每帧每个个体都执行的代码）中的小函数应标记 `inline` 或直接内联
- 禁止在热路径中使用虚函数（`virtual`），包括通过接口/抽象基类调用
- 避免在循环内调用多层封装的小函数；将循环体逻辑展开到调用处

```cpp
// 正确: 直接内联计算，无虚函数
struct Boid {
    float x, y;
    float vx, vy;
};

void updateBoids(std::vector<Boid>& boids, float dt) {
    for (auto& b : boids) {
        // 直接在循环内计算，不通过虚函数或深层调用
        b.x += b.vx * dt;
        b.y += b.vy * dt;
    }
}

// 错误: 热路径中使用虚函数
class IBoid {
public:
    virtual void update(float dt) = 0;  // 每个个体一次虚函数调用
};

// 错误: 循环内频繁调用小函数
for (auto& b : boids) {
    b.setPosition(b.getX() + b.getVelocityX() * dt,   // 4 次函数调用
                  b.getY() + b.getVelocityY() * dt);
}
```

### 0.3 数据布局: SoA 优于 AoS

- 群体属性使用结构体数组（SoA, Structure of Arrays）布局
- 连续的同类型数据对 CPU 缓存和 SIMD 更友好

```cpp
// 正确: SoA 布局，缓存友好
struct FlockData {
    std::vector<float> posX, posY;     // 所有个体的 X 坐标连续存储
    std::vector<float> velX, velY;     // 所有个体的 Y 坐标连续存储
    std::vector<float> sizes;
};

// 避免: AoS 布局，访问单一属性时跳跃读内存
struct Boid {
    float x, y, vx, vy, size;  // 5 个 float 打包
};
std::vector<Boid> boids;  // 遍历所有 x 时跳跃 5 个 float
```

### 0.4 预分配与容量管理

- 所有 `std::vector` 在构造时 `reserve()` 到最大容量
- 使用固定大小数组（`std::array`）代替小 `vector`
- 空间哈希网格的桶数组预分配，每帧 `clear()` 而非重新构造

---

## 1. 渲染规范

### 1.1 必须使用实例化渲染

- 禁止为每个个体单独调用 `glDrawArrays` 或 `glDrawElements`
- 必须使用 `glDrawArraysInstanced` 或 `glDrawElementsInstanced`
- 所有 1000+ 个体在一次（或极少数）绘制调用中完成

```cpp
// 每帧一次绘制调用
glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0, instanceCount);
```

### 1.2 VBO 与实例缓冲更新

- 个体属性（位置、颜色、大小）存储在实例数据缓冲中
- 更新实例缓冲时使用 `glBufferSubData` 或 `glMapBuffer`，禁止重新分配
- 实例数据在 CPU 端使用预分配的 `std::vector<float>` 准备好后一次性上传

### 1.3 纹理优化

- 使用纹理图集合并多个图像到一张大纹理
- 由于需求为固定形状（圆、三角），优先使用纯色/程序化绘制，避免加载纹理

---

## 2. 算法与计算规范

### 2.1 空间哈希网格（必须）

- 邻居搜索必须使用空间哈希网格，将复杂度从 O(n^2) 降到 O(n)
- 网格单元大小 = 个体感知半径 * 1.5
- 网格桶预分配 `std::vector<std::vector<int>>`，每帧清空桶内容但不释放内存

```cpp
// 伪代码: 每帧空间哈希重建
void rebuildGrid(const FlockData& data, float cellSize) {
    for (auto& cell : gridCells) cell.clear();  // 清空，不释放
    for (int i = 0; i < data.count; ++i) {
        int cx = static_cast<int>(data.posX[i] / cellSize);
        int cy = static_cast<int>(data.posY[i] / cellSize);
        gridCells[hash(cx, cy)].push_back(i);
    }
}
```

### 2.2 Reynolds 三规则的高效实现

- 分离、对齐、聚集三个力在同一循环中计算，避免多次遍历个体
- 使用平方距离比较，避免 `sqrt()`

```cpp
// 正确: 一次遍历计算三个力，用平方距离
float dx = otherX - myX;
float dy = otherY - myY;
float distSq = dx * dx + dy * dy;
if (distSq < sepRadiusSq) {
    // 直接用 distSq 计算分离力，不调用 sqrt()
    float factor = 1.0f / (distSq + 0.0001f);
    sepX -= dx * factor;
    sepY -= dy * factor;
}
```

---

## 3. 帧率与性能目标

- 目标帧率: >= 30 FPS（1000 个体，集成显卡）
- 绘制调用次数: <= 10 次/帧（含 UI 和背景）
- 显存占用: <= 256 MB
- 每帧 CPU 堆分配次数: 0
- 每帧动态分配总字节数: 0

---

## 4. 禁止事项清单

- 每帧路径中使用 `new` / `delete` / `malloc` / `free`
- 每帧路径中创建临时 `std::vector` / `std::string` / `std::map`
- 热路径中使用虚函数或函数指针间接调用
- 每帧构造/析构非平凡对象（如 `std::function`、lambda 捕获容器）
- 循环内调用多层级封装的 getter/setter
- 为每个个体单独调用 OpenGL 绘制命令
- 使用 `std::list` 或 `std::map` 存储个体数据（缓存不友好）

---

## 5. 平台要求

- 最低: OpenGL 3.3+（支持实例化渲染）
- C++17 及以上
- 不依赖第三方 ECS 框架或重型库，减少调用栈深度
