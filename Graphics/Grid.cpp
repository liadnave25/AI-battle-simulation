#include "Grid.h"
#include "Definitions.h"
#include <random>
#include <algorithm>
#include <cmath>

using namespace Definitions;

namespace Models {

    const int UI_SAFE_ZONE_ROWS = 15;

    Grid::Grid() {
        clearAll();
        placeObstacles(-1,-1, 0);
        placeDepots(); 
    }

    void Grid::clearAll() {
        for (auto& row : cells) row.fill(Cell::EMPTY);
    }

    static inline bool inBounds(int r, int c) {
        return r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE;
    }

    static inline bool isRiver(int, int c) {
        const int mid = GRID_SIZE / 2;
        return c == mid;
    }

    static inline bool tooCloseToDepot(const Landmarks& lm, int r, int c, int d = 2) {
        auto near = [&](std::pair<int, int> p) {
            return std::abs(r - p.first) + std::abs(c - p.second) <= d;
            };
        return near(lm.ammoBlue) || near(lm.medBlue) || near(lm.ammoOrange) || near(lm.medOrange);
    }

    static inline bool canPlaceHere(const Grid& g, int r, int c) {

        if (r >= GRID_SIZE - UI_SAFE_ZONE_ROWS) return false; 

        if (!inBounds(r, c))               return false;
        if (isRiver(r, c))                 return false;
        if (g.at(r, c) != Cell::EMPTY)     return false;
        if (tooCloseToDepot(g.landmarks(), r, c,2)) return false;
        return true;
    }

    static void placeTreeCluster(Grid& g, int r0, int c0, std::mt19937& rng) {
        if (!canPlaceHere(g, r0, c0)) return;
        g.set(r0, c0, Cell::TREE);

        std::uniform_int_distribution<int> clusterSize(1, 3);
        std::uniform_int_distribution<int> dStep(-1, 1);

        int extra = clusterSize(rng);
        int r = r0, c = c0;
        for (int k = 0; k < extra; ++k) {
            int nr = r + dStep(rng);
            int nc = c + dStep(rng);
            if (canPlaceHere(g, nr, nc)) {
                g.set(nr, nc, Cell::TREE);
                r = nr; c = nc;
            }
        }
    }

    static void placeRock(Grid& g, int r0, int c0, std::mt19937& rng) {
        if (!canPlaceHere(g, r0, c0)) return;
        g.set(r0, c0, Cell::ROCK);

        std::bernoulli_distribution extend(0.35);
        if (extend(rng)) {
            std::uniform_int_distribution<int> dir(0, 3);
            int d = dir(rng);
            int dr[4] = { 1,-1,0,0 };
            int dc[4] = { 0,0,1,-1 };
            int len = 1 + (rng() % 2);
            int r = r0, c = c0;
            for (int k = 0; k < len; ++k) {
                r += dr[d]; c += dc[d];
                if (canPlaceHere(g, r, c)) g.set(r, c, Cell::ROCK);
                else break;
            }
        }
    }

    static void carveRiver(Grid& g) {
        const int mid = GRID_SIZE / 2;
        for (int r = GRID_SIZE / 6; r < GRID_SIZE - GRID_SIZE / 6; ++r)
            g.set(r, mid, Cell::WATER);
    }

    void Grid::placeObstacles(int numTrees, int numRocks, unsigned seed)
    {
        carveRiver(*this);

        const int area = GRID_SIZE * GRID_SIZE;
        if (numTrees < 0) numTrees = std::max(18, area / 70);  
        if (numRocks < 0) numRocks = std::max(15, area / 90);  

        std::mt19937 rng;
        if (seed != 0) rng.seed(seed);
        else           rng.seed(std::random_device{}());

        std::uniform_int_distribution<int> rowDist(0, GRID_SIZE - 1);

        auto randomColOffRiver = [&](std::mt19937& r) {
            const int mid = GRID_SIZE / 2;
            std::bernoulli_distribution side(0.5);
            if (side(r)) {
                std::uniform_int_distribution<int> cdist(0, mid - 2);         
                return cdist(r);
            }
            else {
                std::uniform_int_distribution<int> cdist(mid + 2, GRID_SIZE - 1); 
                return cdist(r);
            }
            };

        {
            int placed = 0, attempts = 0;
            const int maxAttempts = numTrees * 20;
            while (placed < numTrees && attempts < maxAttempts) {
                ++attempts;
                int r = rowDist(rng);
                int c = randomColOffRiver(rng);
                if (!canPlaceHere(*this, r, c)) continue;
                placeTreeCluster(*this, r, c, rng);
                ++placed;
            }
        }

        {
            int placed = 0, attempts = 0;
            const int maxAttempts = numRocks * 20;
            while (placed < numRocks && attempts < maxAttempts) {
                ++attempts;
                int r = rowDist(rng);
                int c = randomColOffRiver(rng);
                if (!canPlaceHere(*this, r, c)) continue;
                placeRock(*this, r, c, rng);
                ++placed;
            }
        }
    }

    void Grid::placeDepots()
    {
        marks.ammoBlue = { GRID_SIZE / 10, GRID_SIZE / 10 };
        marks.medBlue = { GRID_SIZE / 5,  GRID_SIZE / 12 };

        marks.ammoOrange = { GRID_SIZE / 10, GRID_SIZE - GRID_SIZE / 10 };
        marks.medOrange = { GRID_SIZE / 5,  GRID_SIZE - GRID_SIZE / 12 };

        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
                cells[marks.ammoBlue.first + dr][marks.ammoBlue.second + dc] = Cell::EMPTY;
                cells[marks.medBlue.first + dr][marks.medBlue.second + dc] = Cell::EMPTY;
                cells[marks.ammoOrange.first + dr][marks.ammoOrange.second + dc] = Cell::EMPTY;
                cells[marks.medOrange.first + dr][marks.medOrange.second + dc] = Cell::EMPTY;
            }

        cells[marks.ammoBlue.first][marks.ammoBlue.second] = Cell::DEPOT_AMMO;
        cells[marks.medBlue.first][marks.medBlue.second] = Cell::DEPOT_MED;
        cells[marks.ammoOrange.first][marks.ammoOrange.second] = Cell::DEPOT_AMMO;
        cells[marks.medOrange.first][marks.medOrange.second] = Cell::DEPOT_MED;
    }

} // namespace Models
