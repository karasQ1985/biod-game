#include "Renderer.h"
#include "simulation/PlantData.h"
#include <QMatrix4x4>
#include <QImage>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <cmath>

// Quad mesh: boid-shaped silhouette, pointing right, with UV coordinates
static const float MESH_VERTICES[] = {
    // posX, posY,  uvU, uvV
    -0.6f, -0.5f,  0.0f, 1.0f,  // bottom-left
     0.9f, -0.5f,  1.0f, 1.0f,  // bottom-right
     0.9f,  0.5f,  1.0f, 0.0f,  // top-right
    -0.6f,  0.5f,  0.0f, 0.0f,  // top-left
};
static const unsigned int MESH_INDICES[] = { 0, 1, 2, 0, 2, 3 };
static const int MESH_VERTEX_COUNT = 4;
static const int MESH_FLOAT_STRIDE = 4;  // posX, posY, uvU, uvV = 4 floats per vertex

static const char* VERTEX_SHADER = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aOffset;
layout(location = 2) in vec2 aDir;
layout(location = 3) in vec3 aColor;
layout(location = 4) in float aScale;
layout(location = 5) in float aTexLayer;
layout(location = 6) in float aUpright;
layout(location = 7) in vec2 aUV;

uniform mat4 uProj;
uniform float uBoidSize;
uniform float uSpriteScale;

out vec3 vColor;
out float vTexLayer;
out vec2 vUV;

void main() {
    float len = length(aDir);
    vec2 dir = len > 0.001 ? aDir / len : vec2(1.0, 0.0);

    vec2 rotated;
    if (aUpright > 0.5) {
        // Upright mode: keep sprite Y-axis pointing world-up.
        // When moving left (dir.x < 0), mirror horizontally instead of
        // rotating 180 degrees.
        float flipX = (dir.x >= 0.0) ? 1.0 : -1.0;
        float mx = abs(dir.x);
        float mlen = max(length(vec2(mx, dir.y)), 0.001);
        vec2 mdir = vec2(mx, dir.y) / mlen;
        rotated = vec2(
            flipX * (aPos.x * mdir.x - aPos.y * mdir.y),
            aPos.x * mdir.y + aPos.y * mdir.x
        );
    } else {
        // Original full rotation
        rotated = vec2(
            aPos.x * dir.x - aPos.y * dir.y,
            aPos.x * dir.y + aPos.y * dir.x
        );
    }

    // Sprite scale: only applies when texLayer >= 0.5 (has sprite assigned)
    float spriteMul = aTexLayer > 0.5 ? uSpriteScale : 1.0;
    vec2 worldPos = rotated * uBoidSize * aScale * spriteMul + aOffset;
    gl_Position = uProj * vec4(worldPos, 0.0, 1.0);
    vColor = aColor;
    vTexLayer = aTexLayer;
    vUV = aUV;
}
)glsl";

static const char* FRAGMENT_SHADER = R"glsl(
#version 330 core
in vec3 vColor;
in float vTexLayer;
in vec2 vUV;
uniform sampler2DArray uSprites;
out vec4 FragColor;
void main() {
    float useSprite = step(0.5, vTexLayer);
    vec4 texColor = texture(uSprites, vec3(vUV, vTexLayer));
    // Sprite: use raw texture RGB (no color tint). Solid: use boid color.
    vec3 rgb = mix(vColor, texColor.rgb, useSprite);
    float alpha = mix(1.0, texColor.a, useSprite);
    FragColor = vec4(rgb, alpha);
}
)glsl";

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    if (m_vao)  glDeleteVertexArrays(1, &m_vao);
    if (m_meshVBO) glDeleteBuffers(1, &m_meshVBO);
    if (m_meshEBO) glDeleteBuffers(1, &m_meshEBO);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    if (m_spriteTexArray) glDeleteTextures(1, &m_spriteTexArray);
    if (m_program) glDeleteProgram(m_program);
}

void Renderer::init()
{
    initializeOpenGLFunctions();

    m_program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // ---- Mesh VBO (quad with UV coords) ----
    glGenBuffers(1, &m_meshVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(MESH_VERTICES), MESH_VERTICES, GL_STATIC_DRAW);

    // Mesh attribute: aPos (location 0) = vertex position (offset 0, stride 16)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, MESH_FLOAT_STRIDE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Mesh attribute: aUV (location 7) = UV coords (offset 8, stride 16)
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, MESH_FLOAT_STRIDE * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(7);

    // ---- Mesh index buffer ----
    glGenBuffers(1, &m_meshEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(MESH_INDICES), MESH_INDICES, GL_STATIC_DRAW);
    m_meshIndexCount = 6;  // 2 triangles per quad

    // ---- Instance VBO: 10 floats per boid ----
    // {posX, posY, velX, velY, r, g, b, scale, texLayer, upright}
    const int INSTANCE_STRIDE = 10;
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 10000 * INSTANCE_STRIDE * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Instance attribute: aOffset (location 1) = (posX, posY)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // Instance attribute: aDir (location 2) = (velX, velY)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // Instance attribute: aColor (location 3) = (r, g, b)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float),
                          (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    // Instance attribute: aScale (location 4) = pre-computed age*weight scale
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float),
                          (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    // Instance attribute: aTexLayer (location 5) = sprite layer index (0=no sprite)
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float),
                          (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    // Instance attribute: aUpright (location 6) = upright mode flag (0=off, 1=on)
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE * sizeof(float),
                          (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);

    m_instanceData.reserve(10000 * INSTANCE_STRIDE);

    // Create a minimal 1-layer sprite texture array (layer 0 = white, for "no sprite" fallback)
    createSpriteArray(1);
}

void Renderer::createSpriteArray(int numLayers)
{
    if (m_spriteTexArray) glDeleteTextures(1, &m_spriteTexArray);
    glGenTextures(1, &m_spriteTexArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_spriteTexArray);

    // Allocate RGBA8 array: 64x64 per layer
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 64, 64, numLayers, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Fill layer 0 with white (for "no sprite" fallback in shader)
    std::vector<unsigned char> white(64 * 64 * 4, 255);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                    64, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    m_spriteLayerCount = numLayers;
}

int Renderer::loadSprites(const QStringList& paths)
{
    if (paths.isEmpty()) {
        m_spriteNames.clear();
        m_spritePaths.clear();
        m_spriteHashes.clear();
        createSpriteArray(1);
        return 0;
    }

    m_spriteNames.clear();
    m_spritePaths.clear();
    m_spriteHashes.clear();

    // Load all PNGs to memory first
    std::vector<QImage> images;
    for (const auto& path : paths) {
        QImage imgSrc(path);
        if (imgSrc.isNull()) continue;
        // Convert, scale, then flip Y (QImage stores top-to-bottom, OpenGL expects bottom-to-top)
        QImage img = imgSrc.convertToFormat(QImage::Format_RGBA8888)
                        .scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img = img.flipped(Qt::Vertical);
        images.push_back(std::move(img));
        QString fileName = QFileInfo(path).fileName();
        m_spriteNames.push_back(fileName.toStdString());
        m_spritePaths.push_back(path);

        // Compute MD5 hash for change detection
        QFile f(path);
        QByteArray hash;
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash hasher(QCryptographicHash::Md5);
            hasher.addData(&f);
            hash = hasher.result();
        }
        m_spriteHashes.push_back(hash);
    }

    // Recreate texture array: 1 base layer + N sprite layers
    int totalLayers = 1 + static_cast<int>(images.size());
    createSpriteArray(totalLayers);

    for (int i = 0; i < static_cast<int>(images.size()); ++i) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i + 1,
                        64, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE, images[i].bits());
    }
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    return static_cast<int>(m_spriteNames.size());
}

int Renderer::spriteLayer(const std::string& name) const
{
    if (name.empty()) return 0;
    for (size_t i = 0; i < m_spriteNames.size(); ++i) {
        if (m_spriteNames[i] == name) return static_cast<int>(i) + 1;
    }
    return 0;  // not found → no sprite
}

bool Renderer::spritesChanged(const QStringList& paths) const
{
    // Different file count → definitely changed
    if (paths.size() != static_cast<int>(m_spritePaths.size()))
        return true;

    // Compare paths and MD5 hashes
    for (int i = 0; i < paths.size(); ++i) {
        if (paths[i] != m_spritePaths[i])
            return true;
        // Compute current hash and compare
        QFile f(paths[i]);
        if (!f.open(QIODevice::ReadOnly))
            return true;  // can't read → treat as changed
        QCryptographicHash hasher(QCryptographicHash::Md5);
        hasher.addData(&f);
        if (hasher.result() != m_spriteHashes[i])
            return true;
    }
    return false;
}

void Renderer::resize(int width, int height)
{
    m_viewWidth = width;
    m_viewHeight = height;
}

void Renderer::render(const FlockData& data, const PlantData& plants, float worldW, float worldH,
                      uint64_t frameIndex, bool hungerFlashEnabled,
                      const std::vector<std::string>& flockSpriteNames,
                      const std::vector<bool>& flockUprightFlags)
{
    glUseProgram(m_program);

    QMatrix4x4 proj;
    float halfW = worldW / (2.0f * m_viewZoom);
    float halfH = worldH / (2.0f * m_viewZoom);
    proj.ortho(m_viewCenterX - halfW, m_viewCenterX + halfW,
               m_viewCenterY + halfH, m_viewCenterY - halfH,
               -1.0f, 1.0f);
    GLint uProjLoc = glGetUniformLocation(m_program, "uProj");
    GLint uSizeLoc = glGetUniformLocation(m_program, "uBoidSize");
    GLint uTexLoc  = glGetUniformLocation(m_program, "uSprites");
    GLint uSpriteScaleLoc = glGetUniformLocation(m_program, "uSpriteScale");
    glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, proj.constData());
    glUniform1f(uSpriteScaleLoc, m_spriteScale);

    // Bind sprite texture array to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_spriteTexArray);
    glUniform1i(uTexLoc, 0);

    // Enable alpha blending for sprite transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Boid rendering ----
    if (data.count > 0) {
        const int stride = 10;
        int neededSize = data.count * stride;
        m_instanceData.resize(neededSize);
        const float defaultAdultAge = 240.0f;
        for (int i = 0; i < data.count; ++i) {
            int off = i * stride;
            m_instanceData[off + 0] = data.posX[i];
            m_instanceData[off + 1] = data.posY[i];
            m_instanceData[off + 2] = data.velX[i];
            m_instanceData[off + 3] = data.velY[i];

            // Resolve sprite layer and upright flag per-flock
            float layer = 0.0f;
            float upright = 0.0f;
            int fid = data.flockId[i];
            if (fid >= 0 && fid < static_cast<int>(flockSpriteNames.size())) {
                const std::string& name = flockSpriteNames[fid];
                if (!name.empty())
                    layer = static_cast<float>(spriteLayer(name));
                if (fid < static_cast<int>(flockUprightFlags.size()) && flockUprightFlags[fid])
                    upright = 1.0f;
            }

            // Flash red when starving (skip for sprited boids: sprite replaces flash)
            bool flash = hungerFlashEnabled && (data.hunger[i] < 0.30f)
                      && ((frameIndex / 15) % 2 == 1) && (layer == 0.0f);
            if (flash) {
                m_instanceData[off + 4] = 1.0f;
                m_instanceData[off + 5] = 0.15f;
                m_instanceData[off + 6] = 0.15f;
            } else {
                m_instanceData[off + 4] = data.colorR[i];
                m_instanceData[off + 5] = data.colorG[i];
                m_instanceData[off + 6] = data.colorB[i];
            }
            // Combined scale: age-based growth * weight-based size
            float ageFrac = std::min(data.age[i] / defaultAdultAge, 1.0f);
            float ageScale = 0.4f + 0.6f * ageFrac;
            float weightScale = std::sqrt(data.weight[i]);
            m_instanceData[off + 7] = ageScale * weightScale;
            m_instanceData[off + 8] = layer;
            m_instanceData[off + 9] = upright;
        }

        glUniform1f(uSizeLoc, m_boidSize);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, neededSize * sizeof(float), m_instanceData.data());
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_meshIndexCount, GL_UNSIGNED_INT,
                                0, data.count);
    }

    // ---- Plant rendering ----
    int alivePlants = plants.aliveCount();
    if (alivePlants > 0) {
        const int stride = 10;
        int neededSize = alivePlants * stride;
        m_instanceData.resize(neededSize);
        int idx = 0;
        for (int p = 0; p < plants.count; ++p) {
            if (plants.growth[p] <= 0.05f) continue;
            int off = idx * stride;
            m_instanceData[off + 0] = plants.posX[p];
            m_instanceData[off + 1] = plants.posY[p];
            m_instanceData[off + 2] = 1.0f;   // Fixed direction (right)
            m_instanceData[off + 3] = 0.0f;
            // Green color, brightness based on growth
            float g = plants.growth[p];
            m_instanceData[off + 4] = 0.15f * g;
            m_instanceData[off + 5] = 0.3f + 0.5f * g;
            m_instanceData[off + 6] = 0.1f * g;
            m_instanceData[off + 7] = 1.0f;   // Plants: always full scale
            m_instanceData[off + 8] = 0.0f;   // Plants: no sprite
            m_instanceData[off + 9] = 0.0f;   // Plants: no upright
            ++idx;
        }

        float plantSize = m_boidSize * 0.6f;
        glUniform1f(uSizeLoc, plantSize);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, neededSize * sizeof(float), m_instanceData.data());
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_meshIndexCount, GL_UNSIGNED_INT,
                                0, alivePlants);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

GLuint Renderer::compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Renderer::createProgram(const char* vertexSrc, const char* fragmentSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}
