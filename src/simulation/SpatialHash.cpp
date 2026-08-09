#include "SpatialHash.h"
#include <algorithm>
#include <cmath>

SpatialHash::SpatialHash()
    : m_cellSize(60.0f)
    , m_invCellSize(1.0f / 60.0f)
    , m_gridW(0)
    , m_gridH(0)
{
}

void SpatialHash::init(float worldW, float worldH, float cellSize, int maxBoids)
{
    m_cellSize = cellSize;
    m_invCellSize = 1.0f / cellSize;
    m_gridW = static_cast<int>(std::ceil(worldW * m_invCellSize)) + 1;
    m_gridH = static_cast<int>(std::ceil(worldH * m_invCellSize)) + 1;

    int totalCells = m_gridW * m_gridH;
    m_cells.resize(totalCells);
    for (auto& cell : m_cells) {
        cell.reserve(maxBoids / totalCells + 4);
    }
}

void SpatialHash::reinit(float worldW, float worldH, float cellSize, int maxBoids)
{
    // Same as init(), but can be called multiple times to resize the grid.
    init(worldW, worldH, cellSize, maxBoids);
}

void SpatialHash::rebuild(const FlockData& data)
{
    // Clear all cells without releasing memory
    for (auto& cell : m_cells) {
        cell.clear();
    }

    // Insert each boid into its cell
    for (int i = 0; i < data.count; ++i) {
        int cx = static_cast<int>(data.posX[i] * m_invCellSize);
        int cy = static_cast<int>(data.posY[i] * m_invCellSize);
        // Clamp to valid range (boids wrapping at boundaries may go slightly out)
        cx = std::clamp(cx, 0, m_gridW - 1);
        cy = std::clamp(cy, 0, m_gridH - 1);
        int idx = cellIndex(cx, cy);
        m_cells[idx].push_back(i);
    }
}

void SpatialHash::queryNeighbors(float x, float y, float radius,
                                  const FlockData& data,
                                  std::vector<int>& result) const
{
    int cx = static_cast<int>(x * m_invCellSize);
    int cy = static_cast<int>(y * m_invCellSize);

    float radiusSq = radius * radius;

    int minCX = std::max(cx - 1, 0);
    int maxCX = std::min(cx + 1, m_gridW - 1);
    int minCY = std::max(cy - 1, 0);
    int maxCY = std::min(cy + 1, m_gridH - 1);

    for (int gy = minCY; gy <= maxCY; ++gy) {
        for (int gx = minCX; gx <= maxCX; ++gx) {
            const auto& cell = m_cells[cellIndex(gx, gy)];
            for (int idx : cell) {
                float dx = data.posX[idx] - x;
                float dy = data.posY[idx] - y;
                float distSq = dx * dx + dy * dy;
                if (distSq <= radiusSq) {
                    result.push_back(idx);
                }
            }
        }
    }
}

int SpatialHash::cellIndex(int cx, int cy) const
{
    return cy * m_gridW + cx;
}
