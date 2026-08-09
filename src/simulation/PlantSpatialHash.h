#pragma once

#include "PlantData.h"
#include <vector>

// Spatial hash grid for O(n) plant queries.
// Reuses the same cell-based approach as SpatialHash but operates on PlantData.
// Plants are static, so rebuild is only needed when positions change (fertilization/growth).

class PlantSpatialHash {
public:
    PlantSpatialHash() = default;

    // Configure grid dimensions. cellSize should be at least eatRange so a 3x3
    // cell block covers the full foraging / grazing radius.
    void init(float worldW, float worldH, float cellSize, int maxPlants);

    // Rebuild grid from plant positions. Skips dead/unplanted slots (growth <= 0).
    void rebuild(const PlantData& plants);

    // Query neighboring plant indices within radius around (x, y).
    // Only returns plants with growth > 0.0f (alive or regrowing).
    void queryNeighbors(float x, float y, float radius,
                        const PlantData& plants,
                        std::vector<int>& result) const;

private:
    float m_cellSize = 20.0f;
    float m_invCellSize = 0.05f;
    int m_gridW = 0;
    int m_gridH = 0;

    // Pre-allocated cell buckets
    std::vector<std::vector<int>> m_cells;

    int cellIndex(int cx, int cy) const;
    int posToCell(float p) const;
};
