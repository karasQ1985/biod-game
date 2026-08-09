# Biod - Flock Behavior Simulator

A real-time 2D boid simulation based on Reynolds' flocking model, built with C++17 / Qt6 / OpenGL 3.3.

## Features

### Core Simulation
- **Reynolds Rules**: Separation, Alignment, Cohesion with independent per-flock weights
- **Hard Collision**: Close-range repulsion prevents boid stacking, adjustable radius (0-100 px)
- **Boundary Avoidance**: Soft repulsion from world edges with toroidal wrapping as fallback
- **Random Wander**: Per-frame random directional impulse for organic movement
- **Spatial Hash Grid**: O(n) neighbor search with adaptive cell size

### Multi-Flock System
- Dynamic flock add/remove (global cap 10,000, per-flock cap 2,000)
- **Predator-Prey**: Directional chasing, fear-based fleeing, kill streak weight system
- Neutral inter-flock repulsion, custom names and colors
- Male/female color differentiation with toggle

### Hunger & Weight System
- Continuous hunger decay, below-threshold foraging and flash warning
- Dynamic speed modulation based on satiety (invertible curve)
- Kill Streak: consecutive kills build body mass, idle decay over time
- Weight affects movement speed and rendering size

### Plant Ecology
- Random plant growth, spread, and seasonal variation
- Fertilization: predator kills boost nearby plant growth
- **PlantSpatialHash**: Optimizes foraging/grazing/fertilization from O(boids x plants) to O(boids x nearby)

### Reproduction
- Sex-based pairing (male-female), per-flock parameters
- Random offspring count, interval control, minimum hunger requirement, flock size cap

### Rendering
- **Single Draw Call**: `glDrawElementsInstanced` for all boids
- **Sprite System**: `GL_TEXTURE_2D_ARRAY` texture array, per-flock sprites
- Upright sprite mode: images always face Y-up regardless of movement direction
- Sprite scale slider (2x-100x), hunger flash effect
- View zoom: mouse wheel zoom centered on cursor (0.1x-10x)

### UI
- **Centralized ParamRegistry**: One-line parameter registration
- Auto-stepping: keyboard step size derived from value range
- Persistent value labels: real-time display next to each slider
- Right-click input: context menu for precise numeric entry
- Bilingual slider labels (Chinese / English)

## Architecture

```
src/
  main.cpp                    Entry point
  core/
    ParamDef.h                Parameter definition, scale modes
    ParamRegistry.h/.cpp      Centralized parameter registry
  simulation/
    FlockData.h               SoA data layout, FlockParams
    PlantData.h               Plant SoA data
    SpatialHash.h/.cpp        Boid spatial hash grid
    PlantSpatialHash.h/.cpp   Plant spatial hash grid
    Simulation.h/.cpp         Physics update, Reynolds rules, reproduction
  renderer/
    Renderer.h/.cpp           Instanced rendering, sprite texture management
  ui/
    GLWidget.h/.cpp           OpenGL canvas, mouse interaction
    MainWindow.h/.cpp         Toolbar, parameter panels, flock management
```

### Key Design Decisions
- **SoA Layout**: Separate arrays for position, velocity, color for cache efficiency
- **Zero Per-Frame Heap Allocation**: All buffers `reserve()` + `clear()`, never `new`/`delete`
- **Single-Pass Update**: All behaviors computed in one boid loop
- **Single Draw Call**: Instanced rendering independent of boid count

## Build

**Requirements**: CMake 3.16+, Qt 6 (Widgets + OpenGLWidgets), OpenGL 3.3+

```bash
mkdir build && cd build
cmake .. -G Ninja
cmake --build .
```

Windows deployment:
```bash
windeployqt Biod.exe
# Manually copy MinGW runtime DLLs: libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll
```

Sprite images (64x64 transparent PNG) should be placed in the `image/` directory alongside the executable.

## Performance

Tested on Windows 10 with Intel integrated GPU.

| Scenario | Boids | FPS | Physics | Render |
|----------|-------|-----|---------|--------|
| Single flock | 500 | ~200 | ~1.3ms | ~0.5ms |
| Single flock | 3,000 | ~115-184 | ~2-6ms | ~1.5ms |
| Single flock | 5,000 | ~80-130 | ~5-11ms | ~2.5ms |
| Multi-flock (3x500) | 1,500 | ~150-200 | ~2-4ms | ~1.0ms |

## Known Limitations

1. No obstacle avoidance beyond world boundaries
2. Performance log writes block UI thread briefly each second
3. ParamDef only supports float fields; non-float params require manual extension

## Roadmap

- [ ] Static/dynamic obstacle avoidance
- [ ] Compute shader offload for 10,000+ boids
- [ ] Save/load flock configurations
- [ ] Simulation recording and playback
