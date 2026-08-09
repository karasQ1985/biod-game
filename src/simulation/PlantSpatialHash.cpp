#include "PlantSpatialHash.h"
#include <algorithm>
#include <cmath>

void PlantSpatialHash::init(float worldW, float worldH, float cellSize, int maxPlants)
{
    m_cellSize = cellSize;
    m_invCellSize = 1.0f / cellSize;
    m_gridW = static_cast<int>(std::ceil(worldW * m_invCellSize)) + 1;
    m_gridH = static_cast<int>(std::ceil(worldH * m_invCellSize)) + 1;

    int totalCells = m_gridW * m_gridH;
    m_cells.resize(totalCells);
    for (auto& cell : m_cells) {
        cell.reserve(maxPlants / totalCells + 4);
    }
}

void PlantSpatialHash::rebuild(const PlantData& plants)
{
    for (auto& cell : m_cells) {
        cell.clear();
    }

    for (int i = 0; i < plants.count; ++i) {
        if (plants.growth[i] <= 0.0f) continue;  // skip dead/unused slots
        int cx = static_cast<int>(plants.posX[i] * m_invCellSize);
        int cy = static_cast<int>(plants.posY[i] * m_invCellSize);
        cx = std::clamp(cx, 0, m_gridW - 1);
        cy = std::clamp(cy, 0, m_gridH - 1);
        int idx = cellIndex(cx, cy);
        m_cells[idx].push_back(i);
    }
}

void PlantSpatialHash::queryNeighbors(float x, float y, float radius,
                                       const PlantData& plants,
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
                if (plants.growth[idx] <= 0.05f) continue;
                float dx = plants.posX[idx] - x;
                float dy = plants.posY[idx] - y;
                float distSq = dx * dx + dy * dy;
                if (distSq <= radiusSq) {
                    result.push_back(idx);
                }
            }
        }
    }
}

int PlantSpatialHash::cellIndex(int cx, int cy) const
{
    return cy * m_gridW + cx;
}

int PlantSpatialHash::posToCell(float p) const
{
    return static_cast<int>(p * m_invCellSize);
}
