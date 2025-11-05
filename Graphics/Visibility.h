#pragma once
#include <array>
#include <vector>
#include <cstdint>

#include "Definitions.h"
#include "Grid.h"
#include "Units.h"

namespace AI {

    class Visibility {
    public:
        using BArray = std::array<std::array<uint8_t, Definitions::GRID_SIZE>, Definitions::GRID_SIZE>;

        static bool HasLineOfSight(const Models::Grid& map, int r0, int c0, int r1, int c1);

        static void BuildUnitVisibility(const Models::Grid& map, int r, int c, int sightRange, BArray& out);

        static void BuildTeamVisibility(const Models::Grid& map,
            const std::vector<Models::Unit*>& units,
            Definitions::Team team,
            int sightRange,
            BArray& out);

        static void Clear(BArray& arr);
    };

} // namespace AI