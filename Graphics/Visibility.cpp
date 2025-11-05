#include "Visibility.h"
#include <cmath>
#include <algorithm>

using namespace Definitions;

namespace AI {

    void Visibility::Clear(BArray& arr) {
        for (auto& row : arr) row.fill(0);
    }

    static inline bool blocksLOS(int cell) {
        return (cell == ROCK);
    }

    bool Visibility::HasLineOfSight(const Models::Grid& map, int r0, int c0, int r1, int c1)
    {
        int dr = std::abs(r1 - r0);
        int dc = std::abs(c1 - c0);
        int sr = (r0 < r1) ? 1 : -1;
        int sc = (c0 < c1) ? 1 : -1;
        int err = (dc > dr ? dc : -dr) / 2;
        int r = r0, c = c0;

        while (true) {
            if (!(r == r0 && c == c0) && blocksLOS(map.at(r, c))) return false;
            if (r == r1 && c == c1) break;

            int e2 = err;
            if (e2 > -dc) { err -= dr; c += sc; }
            if (e2 < dr) { err += dc; r += sr; }

            if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) break;
        }
        return true;
    }

    void Visibility::BuildUnitVisibility(const Models::Grid& map, int r, int c, int sightRange, BArray& out)
    {
        Clear(out);
        const int R2 = sightRange * sightRange;

        int rmin = std::max(0, r - sightRange);
        int rmax = std::min(GRID_SIZE - 1, r + sightRange);
        int cmin = std::max(0, c - sightRange);
        int cmax = std::min(GRID_SIZE - 1, c + sightRange);

        for (int rr = rmin; rr <= rmax; ++rr) {
            for (int cc = cmin; cc <= cmax; ++cc) {
                int drr = rr - r, dcc = cc - c;
                if (drr * drr + dcc * dcc > R2) continue;
                if (HasLineOfSight(map, r, c, rr, cc)) out[rr][cc] = 1;
            }
        }
    }

    void Visibility::BuildTeamVisibility(const Models::Grid& map,
        const std::vector<Models::Unit*>& units,
        Team team,
        int sightRange,
        BArray& out)
    {
        Clear(out);
        BArray tmp;
        for (size_t i = 0; i < units.size(); ++i) {
            const Models::Unit* u = units[i];
            if (!u->isAlive || u->team != team) continue;
            BuildUnitVisibility(map, u->row, u->col, sightRange, tmp);
            for (int r = 0; r < GRID_SIZE; ++r)
                for (int c = 0; c < GRID_SIZE; ++c)
                    if (tmp[r][c]) out[r][c] = 1;
        }
    }

} // namespace AI
