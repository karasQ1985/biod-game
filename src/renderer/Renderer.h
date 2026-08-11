#pragma once

#include "simulation/FlockData.h"
#include "simulation/PlantData.h"
#include "simulation/NestData.h"
#include <vector>
#include <string>
#include <QOpenGLFunctions_3_3_Core>
#include <QStringList>

// OpenGL instanced renderer for boids.
// Uses a single draw call (glDrawElementsInstanced) for all boids.
// Supports per-flock sprite textures via GL_TEXTURE_2D_ARRAY.
// Instance VBO: 10 floats per boid {posX, posY, velX, velY, r, g, b, scale, texLayer, upright}

class Renderer : protected QOpenGLFunctions_3_3_Core {
public:
    Renderer();
    ~Renderer();

    void init();
    void resize(int width, int height);
    void render(const FlockData& data, const PlantData& plants, const NestData& nests,
                const std::vector<float>& flockColorR,
                const std::vector<float>& flockColorG,
                const std::vector<float>& flockColorB,
                float worldW, float worldH,
                float simTime = 0.0f,
                uint64_t frameIndex = 0, bool hungerFlashEnabled = true,
                const std::vector<std::string>& flockSpriteNames = {},
                const std::vector<bool>& flockUprightFlags = {},
                const std::vector<float>& flockAgeSizes = {},
                const std::vector<float>& flockSexSizes = {});

    void setBoidSize(float size) { m_boidSize = size; }
    float boidSize() const { return m_boidSize; }

    // View control: zoom + pan
    void setViewZoom(float zoom) { m_viewZoom = zoom; }
    float viewZoom() const { return m_viewZoom; }
    void setViewCenter(float cx, float cy) { m_viewCenterX = cx; m_viewCenterY = cy; }
    float viewCenterX() const { return m_viewCenterX; }
    float viewCenterY() const { return m_viewCenterY; }

    // Sprite rendering scale multiplier (2.0 - 100.0)
    void setSpriteScale(float s) { m_spriteScale = s; }
    float spriteScale() const { return m_spriteScale; }

    // Sprite texture management
    // Loads PNG files from paths, creates 64x64 RGBA8 texture array.
    // Returns number of sprites loaded. Layer 0 = white fallback (no sprite).
    int  loadSprites(const QStringList& paths);

    // Returns texture array layer index for named sprite (1-based). 0 = not found / no sprite.
    int  spriteLayer(const std::string& name) const;

    // Check whether any sprite file has changed (count / path / MD5 hash)
    bool spritesChanged(const QStringList& currentPaths) const;

    const std::vector<std::string>& spriteNames() const { return m_spriteNames; }

private:
    GLuint m_vao = 0;
    GLuint m_meshVBO = 0;
    GLuint m_meshEBO = 0;
    GLuint m_instanceVBO = 0;
    GLuint m_program = 0;
    GLuint m_spriteTexArray = 0;

    int m_meshIndexCount = 0;
    float m_boidSize = 6.0f;
    int m_spriteLayerCount = 1;

    // View control
    float m_viewZoom = 1.0f;
    float m_viewCenterX = 960.0f;
    float m_viewCenterY = 540.0f;

    // Sprite scale multiplier
    float m_spriteScale = 2.0f;

    // CPU-side instance buffer: 10 floats per boid
    std::vector<float> m_instanceData;

    // Sprite name→layer mapping (order-sensitive, layer = index + 1)
    std::vector<std::string> m_spriteNames;
    std::vector<QString> m_spritePaths;      // file paths for hash comparison
    std::vector<QByteArray> m_spriteHashes;  // MD5 hashes for change detection

    int m_viewWidth = 800;
    int m_viewHeight = 600;

    void createSpriteArray(int numLayers);
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vertexSrc, const char* fragmentSrc);
};
