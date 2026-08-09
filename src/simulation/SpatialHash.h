#pragma once

#include "FlockData.h"
#include <vector>

// Spatial hash grid for O(n) neighbor queries.
// Cells are pre-allocated; each frame only clears cell contents, never reallocates.

class SpatialHash {
public:
    SpatialHash();

    // Configure grid dimensions. Must be called once before use.
    // worldW / cellSize determines grid resolution.
    void init(float worldW, float worldH, float cellSize, int maxBoids);

    // Reconfigure grid with new cell size (e.g., when perception radius changes).
    // Reallocates internal cell buffers.
    void reinit(float worldW, float worldH, float cellSize, int maxBoids);

    // Rebuild the grid for the current frame: clears all cells, inserts each boid.
    void rebuild(const FlockData& data);

    // Query neighbors within radius around (x, y).
    // Appends neighboring boid indices to result (caller must clear result before first use per frame).
    // Checks a 3x3 block of cells centered on the query point.
    void queryNeighbors(float x, float y, float radius,
                        const FlockData& data,
                        std::vector<int>& result) const;

    int gridWidth() const { return m_gridW; }
    int gridHeight() const { return m_gridH; }

private:
    float m_cellSize;
    float m_invCellSize;
    int m_gridW;
    int m_gridH;

    // Pre-allocated cell buckets: each cell holds a list of boid indices.
    std::vector<std::vector<int>> m_cells;

    int cellIndex(int cx, int cy) const;
    int posToCell(float p, float worldMin) const;
};
