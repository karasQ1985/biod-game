#include "Renderer.h"
#include "simulation/PlantData.h"
#include "core/WorldConstants.h"
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
uniform float uAmbientLight;
out vec4 FragColor;
void main() {
    float useSprite = step(0.5, vTexLayer);
    vec4 texColor = texture(uSprites, vec3(vUV, vTexLayer));
    // Sprite: use raw texture RGB (no color tint). Solid: use boid color.
    vec3 rgb = mix(vColor, texColor.rgb, useSprite);
    float alpha = mix(1.0, texColor.a, useSprite);
    rgb *= uAmbientLight;
    FragColor = vec4(rgb, alpha);
}
)glsl";

// -------------------- Phase 3.6: Terrain shaders --------------------

// Terrain bake: Simplex noise → FBO texture
static const char* TERRAIN_BAKE_VS = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vCoord;
void main() {
    vCoord = aPos;
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

static const char* TERRAIN_BAKE_FS = R"glsl(
#version 330 core
in vec2 vCoord;
out vec4 FragColor;

uniform float uLatitude;   // -90 (S pole) to +90 (N pole)
uniform float uWorldW;     // world pixel width
uniform float uWorldH;     // world pixel height

vec3 mod289(vec3 x) { return x - floor(x / 289.0) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x / 289.0) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }

float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                         -0.577350269189626, 0.024390243902439);
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod289(i);
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0))
                     + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
                            dot(x12.zw,x12.zw)), 0.0);
    m = m*m; m = m*m;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);
    vec3 g;
    g.x  = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.65;
    for (int i = 0; i < 5; i++) { v += a * snoise(p); p *= 2.1; a *= 0.45; }
    return v * 0.5 + 0.5;
}

float moistureNoise(vec2 p) {
    float v = 0.0;
    float a = 0.6;
    for (int i = 0; i < 4; i++) { v += a * snoise(p + 100.0); p *= 2.1; a *= 0.45; }
    return v * 0.5 + 0.5;
}

void main() {
    // Noise sampled per-world-pixel: same physical feature size in X and Y
    // e.g. 1300x200 world: X goes 0→3.25, Y goes 0→0.5 → features ~400px diameter
    const float pixelsPerNoiseUnit = 400.0;
    vec2 pos = vCoord * vec2(uWorldW, uWorldH) / pixelsPerNoiseUnit;
    float elev = fbm(pos);
    float moist = moistureNoise(pos);

    // Latitude → uniform climate temperature factor (entire map, not per-pixel)
    // 0 = pole (cold), 1 = equator (hot)
    float absLat = abs(uLatitude);
    float tempFactor = 1.0 - smoothstep(0.0, 90.0, absLat);
    tempFactor = clamp(tempFactor, 0.15, 1.0);

    // Biome palette
    vec3 deepWater = vec3(0.10, 0.14, 0.30);
    vec3 water     = vec3(0.13, 0.20, 0.38);
    vec3 sand      = vec3(0.55, 0.52, 0.40);
    vec3 grass     = vec3(0.32, 0.45, 0.28);
    vec3 forest    = vec3(0.18, 0.28, 0.20);
    vec3 rock      = vec3(0.35, 0.33, 0.30);
    vec3 snowPeak  = vec3(0.65, 0.63, 0.62);
    // Cold-climate colors
    vec3 tundraCol = vec3(0.55, 0.60, 0.65);
    vec3 iceCol    = vec3(0.75, 0.80, 0.85);

    // Elevation thresholds (shifted by temperature for snowline)
    float coldShift = (1.0 - tempFactor) * 0.25;
    float t0 = smoothstep(0.08 - coldShift, 0.15 + coldShift, elev);
    float t1 = smoothstep(0.22 - coldShift, 0.30, elev);
    float t2 = smoothstep(0.35, 0.45 + coldShift * 0.5, elev);
    float t3 = smoothstep(0.55, 0.70, elev);
    float t4 = smoothstep(0.78, 0.88, elev);
    float t5 = smoothstep(0.90, 0.96, elev);

    // Build color with uniform climate influence (no per-pixel gradient)
    vec3 col = deepWater;
    col = mix(col, water,     t0);
    // In cold climates, replace sand with tundra
    vec3 lowGround = mix(sand, tundraCol, 1.0 - tempFactor);
    col = mix(col, lowGround,  t1);
    // Grass vs tundra for mid elevation
    vec3 midGround = mix(grass, mix(tundraCol, grass, 0.5), 1.0 - tempFactor);
    col = mix(col, midGround, t2);
    col = mix(col, forest,    t3);
    col = mix(col, rock,      t4);
    // Snowline lower in cold climates
    float snowMix = mix(0.0, 0.6, 1.0 - tempFactor);
    col = mix(col, mix(snowPeak, iceCol, 1.0 - tempFactor), t5 + snowMix * (1.0 - t5));

    // Moisture boost for vegetation in warm areas
    col = mix(col, col * 1.15, moist * 0.3 * smoothstep(0.30, 0.70, elev) * tempFactor);

    // Slope shading (reduced intensity for boid visibility)
    float d = 1.0/2048.0;
    float eL = fbm(pos + vec2(d, 0)*48.0);
    float eR = fbm(pos - vec2(d, 0)*48.0);
    float eU = fbm(pos + vec2(0, d)*48.0);
    float eD = fbm(pos - vec2(0, d)*48.0);
    col *= 1.0 - length(vec2(eR-eL, eD-eU)) * 0.4;

    // Strong desaturation (~60%) for boid visibility
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(lum), 0.60);

    // Reduce overall brightness so boid colors pop
    col *= 0.75;

    FragColor = vec4(col, 1.0);
}
)glsl";

// Terrain display: samples FBO + day/night color temperature
static const char* TERRAIN_DISPLAY_VS = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uProj;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)glsl";

static const char* TERRAIN_DISPLAY_FS = R"glsl(
#version 330 core
in vec2 vUV;
uniform sampler2D uTerrainTex;
uniform float uDayPhase;
out vec4 FragColor;

void main() {
    vec3 terrain = texture(uTerrainTex, vUV).rgb;
    float df = pow(sin(uDayPhase * 3.14159265), 1.5);
    float bgLight = 0.18 + 0.82 * df;
    vec3 nightTint = vec3(0.15, 0.25, 0.55);
    vec3 dayTint   = vec3(1.05, 1.02, 0.92);
    vec3 bgTint = mix(nightTint, dayTint, df);
    vec3 outColor = terrain * bgTint * bgLight;
    outColor = max(outColor, vec3(0.02));
    FragColor = vec4(outColor, 1.0);
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
    // Phase 3.6: Terrain cleanup
    if (m_terrainVAO) glDeleteVertexArrays(1, &m_terrainVAO);
    if (m_terrainVBO) glDeleteBuffers(1, &m_terrainVBO);
    if (m_terrainTex) glDeleteTextures(1, &m_terrainTex);
    if (m_terrainFBO) glDeleteFramebuffers(1, &m_terrainFBO);
    if (m_terrainProgram) glDeleteProgram(m_terrainProgram);
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
    glBufferData(GL_ARRAY_BUFFER, 20000 * INSTANCE_STRIDE * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

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

    m_instanceData.reserve(20000 * INSTANCE_STRIDE);

    // Create a minimal 1-layer sprite texture array (layer 0 = white, for "no sprite" fallback)
    createSpriteArray(1);

    // Phase 3.6: Terrain display program + quad VAO (FBO created on first bake)
    m_terrainProgram = createProgram(TERRAIN_DISPLAY_VS, TERRAIN_DISPLAY_FS);

    // Terrain display quad VAO (world-space quad + UV)
    glGenVertexArrays(1, &m_terrainVAO);
    glBindVertexArray(m_terrainVAO);
    static const float quadData[] = {
        // posX, posY, uvU, uvV  (pos will be set per-frame, UV is fixed)
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    glGenBuffers(1, &m_terrainVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
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

// Phase 0.7: Terrain config from WorldSetupDialog
void Renderer::setTerrainConfig(float latitude,
                                float waterPct, float forestPct, float grasslandPct,
                                float desertPct, float tundraPct, float mountainPct,
                                float wetlandPct)
{
    m_terrainConfig.latitude      = latitude;
    m_terrainConfig.waterPct      = waterPct;
    m_terrainConfig.forestPct     = forestPct;
    m_terrainConfig.grasslandPct  = grasslandPct;
    m_terrainConfig.desertPct     = desertPct;
    m_terrainConfig.tundraPct     = tundraPct;
    m_terrainConfig.mountainPct   = mountainPct;
    m_terrainConfig.wetlandPct    = wetlandPct;
    m_terrainDirty = true;  // Re-bake next frame
}

// Phase 0.7: Create or resize terrain FBO to match world aspect ratio
void Renderer::createOrResizeTerrainFBO(float worldW, float worldH)
{
    // Compute FBO dimensions (max dimension 2048, proportional smaller dim)
    const int maxDim = 2048;
    int fboW, fboH;
    if (worldW >= worldH) {
        fboW = maxDim;
        fboH = std::max(1, static_cast<int>(maxDim * worldH / worldW));
    } else {
        fboH = maxDim;
        fboW = std::max(1, static_cast<int>(maxDim * worldW / worldH));
    }

    // Only resize if dimensions changed
    if (fboW == m_terrainFBOW && fboH == m_terrainFBOH && m_terrainFBO != 0)
        return;

    // Delete old FBO/texture if they exist
    if (m_terrainTex) glDeleteTextures(1, &m_terrainTex);
    if (m_terrainFBO) glDeleteFramebuffers(1, &m_terrainFBO);

    m_terrainFBOW = fboW;
    m_terrainFBOH = fboH;

    glGenFramebuffers(1, &m_terrainFBO);
    glGenTextures(1, &m_terrainTex);
    glBindTexture(GL_TEXTURE_2D, m_terrainTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboW, fboH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, m_terrainFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_terrainTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_terrainDirty = true;  // Re-bake with new aspect ratio
}

// Phase 3.6: Bake terrain Simplex noise to FBO texture (one-time cost until dirty)
void Renderer::bakeTerrain(float worldW, float worldH)
{
    // Ensure FBO matches current world aspect ratio
    createOrResizeTerrainFBO(worldW, worldH);
    if (!m_terrainFBO || !m_terrainTex) return;

    GLuint bakeProg = createProgram(TERRAIN_BAKE_VS, TERRAIN_BAKE_FS);
    if (!bakeProg) return;

    // Pass terrain config as uniforms
    GLint locLat = glGetUniformLocation(bakeProg, "uLatitude");
    GLint locW   = glGetUniformLocation(bakeProg, "uWorldW");
    GLint locH   = glGetUniformLocation(bakeProg, "uWorldH");
    if (locLat >= 0) glUniform1f(locLat, m_terrainConfig.latitude);
    if (locW >= 0)   glUniform1f(locW, worldW);
    if (locH >= 0)   glUniform1f(locH, worldH);

    // Save state
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, m_terrainFBO);
    glViewport(0, 0, m_terrainFBOW, m_terrainFBOH);
    glUseProgram(bakeProg);

    // Full-texture quad in UV space [0,1]
    glBindVertexArray(m_terrainVAO);
    float bakeQuad[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bakeQuad), bakeQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Restore state
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glDeleteProgram(bakeProg);

    m_terrainDirty = false;
}

// Phase 3.6: Render terrain background quad with day/night tint
void Renderer::renderTerrain(float worldW, float worldH)
{
    if (!m_terrainProgram || !m_terrainTex) return;

    glUseProgram(m_terrainProgram);

    // World-space quad matching simulation bounds
    float quad[] = {
        0.0f,     0.0f,     0.0f, 0.0f,
        worldW,   0.0f,     1.0f, 0.0f,
        worldW,   worldH,   1.0f, 1.0f,
        0.0f,     0.0f,     0.0f, 0.0f,
        worldW,   worldH,   1.0f, 1.0f,
        0.0f,     worldH,   0.0f, 1.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

    // Projection matrix (same as main renderer, aspect-corrected)
    float viewAspect = static_cast<float>(m_viewWidth) / std::max(1, m_viewHeight);
    float worldAspect = worldW / std::max(worldH, 1.0f);
    float halfW, halfH;
    if (viewAspect >= worldAspect) {
        halfH = worldH / (2.0f * m_viewZoom);
        halfW = halfH * viewAspect;
    } else {
        halfW = worldW / (2.0f * m_viewZoom);
        halfH = halfW / viewAspect;
    }
    QMatrix4x4 proj;
    proj.ortho(m_viewCenterX - halfW, m_viewCenterX + halfW,
               m_viewCenterY + halfH, m_viewCenterY - halfH,
               -1.0f, 1.0f);
    GLint uProjLoc = glGetUniformLocation(m_terrainProgram, "uProj");
    glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, proj.constData());

    // Bind terrain texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_terrainTex);
    GLint uTexLoc = glGetUniformLocation(m_terrainProgram, "uTerrainTex");
    glUniform1i(uTexLoc, 0);

    // Day/night phase uniforms
    GLint uDayPhaseLoc = glGetUniformLocation(m_terrainProgram, "uDayPhase");
    // uDayPhase is set in render() before this call

    glBindVertexArray(m_terrainVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::resize(int width, int height)
{
    m_viewWidth = width;
    m_viewHeight = height;
}

void Renderer::render(const FlockData& data, const PlantData& plants, const NestData& nests,
                       const std::vector<float>& flockColorR,
                       const std::vector<float>& flockColorG,
                       const std::vector<float>& flockColorB,
                       float worldW, float worldH,
                       float simTime,
                       uint64_t frameIndex, bool hungerFlashEnabled,
                      const std::vector<std::string>& flockSpriteNames,
                      const std::vector<bool>& flockUprightFlags,
                      const std::vector<float>& flockAgeSizes,
                      const std::vector<float>& flockSexSizes)
{
    // ---- Calculate day/night phase once (shared by terrain + foreground) ----
    float dayPhase = std::fmod(simTime / WorldConst::SECONDS_PER_SIM_DAY, 1.0f);

    // Phase 3.6: Bake terrain to FBO if dirty (one-time or on world change)
    if (m_terrainDirty)
        bakeTerrain(worldW, worldH);

    // Phase 3.6: Render terrain background with day/night tint
    // Set dayPhase uniform before drawing terrain
    glUseProgram(m_terrainProgram);
    GLint uDayPhaseLoc = glGetUniformLocation(m_terrainProgram, "uDayPhase");
    glUniform1f(uDayPhaseLoc, dayPhase);
    glUseProgram(0);
    renderTerrain(worldW, worldH);

    // ---- Boid / plant / nest rendering ----
    glUseProgram(m_program);

    // Day/night: foreground boid/plant/nest with mild dimming (B+C combined)
    float fgLight = 0.5f + 0.5f * std::pow(std::sin(dayPhase * 3.14159265f), 1.2f);
    GLint uAmbientLoc = glGetUniformLocation(m_program, "uAmbientLight");
    glUniform1f(uAmbientLoc, fgLight);

    QMatrix4x4 proj;
    // Aspect-corrected projection: world pixels remain square regardless of viewport shape
    float viewAspect = static_cast<float>(m_viewWidth) / std::max(1, m_viewHeight);
    float worldAspect = worldW / std::max(worldH, 1.0f);
    float halfW, halfH;
    if (viewAspect >= worldAspect) {
        // Viewport wider than world → world fits height, letterbox on sides
        halfH = worldH / (2.0f * m_viewZoom);
        halfW = halfH * viewAspect;
    } else {
        // Viewport taller than world → world fits width, letterbox on top/bottom
        halfW = worldW / (2.0f * m_viewZoom);
        halfH = halfW / viewAspect;
    }
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
            // Combined scale: age-stage-based growth * weight-based size
            float ageScale = 1.0f;
            if (!flockAgeSizes.empty() && fid >= 0) {
                int base = fid * 4;
                if (base + 3 < static_cast<int>(flockAgeSizes.size())) {
                    switch (static_cast<AgeStage>(data.ageStage[i])) {
                    case AgeStage::Juvenile: ageScale = flockAgeSizes[base + 0]; break;
                    case AgeStage::Young:    ageScale = flockAgeSizes[base + 1]; break;
                    case AgeStage::Adult:    ageScale = flockAgeSizes[base + 2]; break;
                    case AgeStage::Elder:    ageScale = flockAgeSizes[base + 3]; break;
                    }
                }
            }
            float weightScale = std::sqrt(data.weight[i]);

            // Sex-based size scaling
            float sexScale = 1.0f;
            if (!flockSexSizes.empty() && fid >= 0) {
                int base = fid * 2;
                if (base + 1 < static_cast<int>(flockSexSizes.size())) {
                    sexScale = (data.sex[i] == 0) ? flockSexSizes[base] : flockSexSizes[base + 1];
                }
            }

            m_instanceData[off + 7] = ageScale * weightScale * sexScale;
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

    // ---- Nest rendering (Phase 3.1) ----
    if (nests.count > 0) {
        int stride = 10;
        int neededSize = nests.count * stride;
        if (static_cast<int>(m_instanceData.size()) < neededSize)
            m_instanceData.resize(neededSize);

        int nFlockColors = static_cast<int>(flockColorR.size());
        for (int n = 0; n < nests.count; ++n) {
            int off = n * stride;
            m_instanceData[off + 0] = nests.posX[n];
            m_instanceData[off + 1] = nests.posY[n];
            // Color: flock color if owned, gray if unowned
            int owner = nests.ownerFlock[n];
            if (owner >= 0 && owner < nFlockColors) {
                m_instanceData[off + 2] = flockColorR[owner];
                m_instanceData[off + 3] = flockColorG[owner];
                m_instanceData[off + 4] = flockColorB[owner];
            } else {
                m_instanceData[off + 2] = 0.5f;  // Gray for unowned
                m_instanceData[off + 3] = 0.5f;
                m_instanceData[off + 4] = 0.5f;
            }
            m_instanceData[off + 5] = 0.0f;
            m_instanceData[off + 6] = 0.0f;
            m_instanceData[off + 7] = 1.0f;    // Full scale
            m_instanceData[off + 8] = 0.0f;    // No sprite
            m_instanceData[off + 9] = 0.0f;    // No upright
        }

        float nestSize = m_boidSize * 2.5f;  // Nests are larger
        glUniform1f(uSizeLoc, nestSize);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, neededSize * sizeof(float), m_instanceData.data());
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_meshIndexCount, GL_UNSIGNED_INT,
                                0, nests.count);
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
