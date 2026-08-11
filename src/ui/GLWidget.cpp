#include "GLWidget.h"
#include "simulation/Simulation.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

GLWidget::GLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Init simulation data early (no GL dependency) so MainWindow
    // can query flock info during setupUI() / setupToolbar()
    m_sim.init(1920.0f, 1080.0f, 10000);
    m_viewZoom = 1.0f;
}

GLWidget::~GLWidget() = default;

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    m_renderer.init();

    // Initialize view: center on world center, zoom = 1.0
    m_renderer.setViewCenter(m_sim.worldW() * 0.5f, m_sim.worldH() * 0.5f);
    m_renderer.setViewZoom(m_viewZoom);

    // Simulation data already initialized in constructor.
    // No auto-spawning -- user adds boids manually via toolbar.

    m_frameTimer.start();
    m_lastFrameNs = m_frameTimer.nsecsElapsed();

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() { update(); });
    timer->start(16);
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_renderer.resize(w, h);
}

void GLWidget::paintGL()
{
    qint64 t0 = m_frameTimer.nsecsElapsed();

    // Compute frame delta time in seconds
    float dt = static_cast<float>(t0 - m_lastFrameNs) * 1e-9f;
    m_lastFrameNs = t0;

    // Clamp dt to avoid spiral of death after pause/resume
    if (dt > 0.1f) dt = 0.016f;

    glClear(GL_COLOR_BUFFER_BIT);

    // ---- Simulation ----
    qint64 t1 = m_frameTimer.nsecsElapsed();
    m_sim.update(dt);
    qint64 t2 = m_frameTimer.nsecsElapsed();

    // ---- Render ----
    m_renderer.render(m_sim.data(), m_sim.plants(), m_sim.nests(),
                      m_sim.flockColorR(), m_sim.flockColorG(), m_sim.flockColorB(),
                      m_sim.worldW(), m_sim.worldH(),
                      m_sim.simTime(),
                      m_frameIndex, m_sim.globalParams().hungerFlashEnabled,
                      m_sim.flockSpriteNames(), m_sim.flockUprightFlags(),
                      m_sim.flockAgeSizes(), m_sim.flockSexSizes());
    qint64 t3 = m_frameTimer.nsecsElapsed();

    // ---- Performance tracking ----
    float simMs = static_cast<float>(t2 - t1) * 1e-6f;
    float renderMs = static_cast<float>(t3 - t2) * 1e-6f;
    float frameMs = static_cast<float>(t3 - t0) * 1e-6f;

    ++m_frameIndex;

    // FPS: average over 500ms windows
    m_fpsAccumNs += (t3 - t0);
    ++m_fpsFrameCount;
    if (m_fpsAccumNs >= 500000000LL) { // 500ms
        m_fps = static_cast<float>(m_fpsFrameCount) /
                (static_cast<float>(m_fpsAccumNs) * 1e-9f);
        m_fpsAccumNs = 0;
        m_fpsFrameCount = 0;
    }

    // Update snapshot for UI thread to read
    m_perfSnapshot.fps = m_fps;
    m_perfSnapshot.simTimeMs = simMs;
    m_perfSnapshot.renderTimeMs = renderMs;
    m_perfSnapshot.frameTimeMs = frameMs;
    m_perfSnapshot.frameCount = m_frameIndex;
    m_perfSnapshot.boidCount = m_sim.data().count;
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    float delta = event->angleDelta().y() / 120.0f;

    // Record current projection state
    float oldZoom = m_viewZoom;
    float oldCX = m_renderer.viewCenterX();
    float oldCY = m_renderer.viewCenterY();

    // Compute mouse position in world coords (under old projection)
    float sx = static_cast<float>(event->position().x()) / width();
    float sy = static_cast<float>(event->position().y()) / height();
    float oldHalfW = m_sim.worldW() / (2.0f * oldZoom);
    float oldHalfH = m_sim.worldH() / (2.0f * oldZoom);
    float mouseWX = oldCX + (sx - 0.5f) * 2.0f * oldHalfW;
    float mouseWY = oldCY + (sy - 0.5f) * 2.0f * oldHalfH;

    // Apply new zoom
    m_viewZoom *= std::pow(1.1f, delta);
    m_viewZoom = std::clamp(m_viewZoom, 0.1f, 10.0f);

    // Recalculate center so mouse world position stays fixed on screen
    float newHalfW = m_sim.worldW() / (2.0f * m_viewZoom);
    float newHalfH = m_sim.worldH() / (2.0f * m_viewZoom);
    float newCX = mouseWX - (sx - 0.5f) * 2.0f * newHalfW;
    float newCY = mouseWY - (sy - 0.5f) * 2.0f * newHalfH;

    m_renderer.setViewZoom(m_viewZoom);
    m_renderer.setViewCenter(newCX, newCY);
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        float sx = static_cast<float>(event->pos().x()) / width();
        float sy = static_cast<float>(event->pos().y()) / height();
        float wx = m_renderer.viewCenterX() + (sx - 0.5f) * m_sim.worldW() / m_viewZoom;
        float wy = m_renderer.viewCenterY() + (sy - 0.5f) * m_sim.worldH() / m_viewZoom;
        m_sim.setTarget(wx, wy);
    } else if (event->button() == Qt::RightButton) {
        m_sim.clearTarget();
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        float sx = static_cast<float>(event->pos().x()) / width();
        float sy = static_cast<float>(event->pos().y()) / height();
        float wx = m_renderer.viewCenterX() + (sx - 0.5f) * m_sim.worldW() / m_viewZoom;
        float wy = m_renderer.viewCenterY() + (sy - 0.5f) * m_sim.worldH() / m_viewZoom;
        m_sim.setTarget(wx, wy);
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void GLWidget::setBoidCount(int count)
{
    int current = m_sim.data().count;
    if (count > current) {
        m_sim.spawnRandom(count - current);
    } else if (count < current) {
        for (int i = current - 1; i >= count; --i) {
            m_sim.removeBoidAt(i);
        }
    }
}

void GLWidget::spawnBoids(int count)
{
    m_sim.spawnRandom(count);
}

void GLWidget::removeRandomBoid()
{
    if (m_sim.data().count > 0) {
        m_sim.removeBoidAt(m_sim.data().count - 1);
    }
}
