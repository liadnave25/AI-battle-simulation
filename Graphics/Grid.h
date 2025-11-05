#pragma once
#include <array>
#include <utility>
#include "Definitions.h"

namespace Models {

    using CellT = int;
    using GridArray = std::array<std::array<CellT, Definitions::GRID_SIZE>, Definitions::GRID_SIZE>;
    using ivec2 = std::pair<int, int>;

    struct Landmarks {
        ivec2 ammoBlue{ 2, 2 };
        ivec2 medBlue{ 2, 5 };
        ivec2 ammoOrange{ Definitions::GRID_SIZE - 3, Definitions::GRID_SIZE - 3 };
        ivec2 medOrange{ Definitions::GRID_SIZE - 3, Definitions::GRID_SIZE - 6 };
    };
    using Landmarks_t = Landmarks;

    class Grid {
    public:
        Grid();

        inline int  size() const { return Definitions::GRID_SIZE; }
        inline CellT at(int r, int c) const { return cells[r][c]; }
        inline void set(int r, int c, CellT v) { cells[r][c] = v; }

        const Landmarks& landmarks() const { return marks; }

    private:
        GridArray  cells{};
        Landmarks  marks;

        void clearAll();
        void placeObstacles(int numTrees = -1, int numRocks = -1, unsigned seed = 0);
        void placeDepots();
    };

} // namespace Models
