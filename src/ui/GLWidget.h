#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QElapsedTimer>
#include "renderer/Renderer.h"
#include "simulation/Simulation.h"

struct PerfStats {
    float fps = 0.0f;
    float simTimeMs = 0.0f;     // Simulation update time in milliseconds
    float renderTimeMs = 0.0f;  // OpenGL render time in milliseconds
    float frameTimeMs = 0.0f;   // Total frame time (sim + render + overhead) in milliseconds
    int frameCount = 0;         // Total frames rendered since start
    int boidCount = 0;          // Current boid count (snapshot)
};

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget() override;

    Simulation& simulation() { return m_sim; }
    const Simulation& simulation() const { return m_sim; }
    Renderer& renderer() { return m_renderer; }

    void setBoidCount(int count);
    int boidCount() const { return m_sim.data().count; }

    // Configure world before initializeGL (called from MainWindow after WorldSetupDialog)
    void configureWorld(float worldW, float worldH,
                        int boundaryMode, float latitude,
                        float waterPct, float forestPct, float grasslandPct,
                        float desertPct, float tundraPct,
                        float mountainPct, float wetlandPct);

    // Resize world dimensions (preserves flock configs, regenerates entities)
    void resizeWorld(float worldW, float worldH);

    // Snapshot current performance stats (thread-safe: called from UI timer)
    PerfStats perfStats() const { return m_perfSnapshot; }

    // Set initial spawn count (must be called before initializeGL)
    void setInitialBoidCount(int count) { m_initialBoidCount = count; }

public slots:
    void spawnBoids(int count);
    void removeRandomBoid();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    Simulation m_sim;
    Renderer m_renderer;
    QElapsedTimer m_frameTimer;

    // Frame timing (nanoseconds)
    qint64 m_lastFrameNs = 0;

    // Performance tracking
    qint64 m_fpsAccumNs = 0;       // Accumulated time for FPS window
    int m_fpsFrameCount = 0;       // Frame count in current FPS window
    float m_fps = 0.0f;            // Smoothed FPS
    PerfStats m_perfSnapshot;      // Snapshot for UI thread to read

    int m_frameIndex = 0;          // Total frame counter

    bool m_dragging = false;
    int m_initialBoidCount = 500;  // Configurable initial count

    // View control
    float m_viewZoom = 1.0f;
};
