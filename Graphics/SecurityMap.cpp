#include "SecurityMap.h"
#include <algorithm>
#include <cmath>
#include <utility> 

using namespace Definitions;

static constexpr float TREE_RISK_FACTOR = 0.65f;

namespace Simulation {

    SecurityMap::SecurityMap() { clear(); }

    void SecurityMap::clear() {
        for (auto& row : smap_) row.fill(0.0f);
    }

    void SecurityMap::fill(float v) {
        for (auto& row : smap_) row.fill(v);
    }

    void SecurityMap::add(int r, int c, float v) {
        if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) return;
        smap_[r][c] += v;
    }

    float SecurityMap::at(int r, int c) const {
        if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) return 0.0f;
        return smap_[r][c];
    }

    float SecurityMap::maxValue() const {
        float m = 0.0f;
        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                m = std::max(m, smap_[r][c]);
        return (m <= 0.0f ? 1.0f : m);
    }

    void SecurityMap::traceRay(const Models::Grid& grid,
        float r0, float c0,
        float dr, float dc,
        float power,
        int maxSteps)
    {
        float r = r0;
        float c = c0;

        const float decay = SECURITY_DECAY; 

        for (int step = 0; step < maxSteps && power > 0.001f; ++step)
        {
            int ri = (int)std::round(r);
            int ci = (int)std::round(c);
            if (ri < 0 || ri >= GRID_SIZE || ci < 0 || ci >= GRID_SIZE) break;


            int cell = grid.at(ri, ci);

            if (cell == ROCK) {
                break; 
            }

            if (cell == TREE) {
                smap_[ri][ci] += power * TREE_RISK_FACTOR;
                power *= TREE_RISK_FACTOR;
            }
            else {
                smap_[ri][ci] += power;
            }


            r += dr;
            c += dc;
            power *= decay; 
        }
    }

    static std::pair<float, float> unitVec(float x, float y)
    {
        float len = std::sqrt(x * x + y * y);
        if (len == 0.0f) return std::make_pair(0.0f, 0.0f);
        return std::make_pair(x / len, y / len);
    }

    void SecurityMap::RebuildSecurityMap(const Models::Grid& grid)
    {
        clear();

        const int S = SECURITY_SAMPLES;             
        const int mid = GRID_SIZE / 2;

        const int maxRange = std::max(FIRE_RANGE, GRENADE_RANGE);
        const int ttl = (int)std::ceil(1.2f * maxRange);

        const float basePower = 1.0f;

        for (int i = 0; i < S; ++i) {
            float r0 = 2 + (i * (GRID_SIZE - 4)) / float(S);
            float c0 = 2;
            float rt = r0 + 15.0f * std::sin(0.31f * i);
            float ct = (float)mid;
            std::pair<float, float> v = unitVec(rt - r0, ct - c0);
            traceRay(grid, r0, c0, v.first, v.second, basePower, ttl);
        }

        for (int i = 0; i < S; ++i) {
            float r0 = 2 + (i * (GRID_SIZE - 4)) / float(S);
            float c0 = GRID_SIZE - 3;
            float rt = r0 + 15.0f * std::cos(0.29f * i);
            float ct = (float)mid;
            std::pair<float, float> v = unitVec(rt - r0, ct - c0);
            traceRay(grid, r0, c0, v.first, v.second, basePower, ttl);
        }

        for (int i = 0; i < S / 2; ++i) {
            float r0 = GRID_SIZE - 3;
            float c0 = 2 + (i * (GRID_SIZE - 4)) / float(S / 2);
            float rt = (float)mid;
            float ct = c0 + 10.0f * std::sin(0.37f * i);
            std::pair<float, float> v = unitVec(rt - r0, ct - c0);
            traceRay(grid, r0, c0, v.first, v.second, basePower * 0.9f, ttl);
        }

        for (int i = 0; i < S / 2; ++i) {
            float r0 = 2;
            float c0 = 2 + (i * (GRID_SIZE - 4)) / float(S / 2);
            float rt = (float)mid;
            float ct = c0 + 10.0f * std::cos(0.41f * i);
            std::pair<float, float> v = unitVec(rt - r0, ct - c0);
            traceRay(grid, r0, c0, v.first, v.second, basePower * 0.9f, ttl);
        }

        for (int i = 0; i < S / 2; ++i) {
            float r0 = 3 + (i * (GRID_SIZE - 6)) / float(S / 2);

            float cL = mid - 3;
            std::pair<float, float> v1 = unitVec(0.0f, +1.0f);
            traceRay(grid, r0, cL, v1.first, v1.second, basePower * 0.6f, ttl / 2);

            float cR = mid + 3;
            std::pair<float, float> v2 = unitVec(0.0f, -1.0f);
            traceRay(grid, r0, cR, v2.first, v2.second, basePower * 0.6f, ttl / 2);
        }
    }

} // namespace Sim
