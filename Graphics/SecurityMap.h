#pragma once
#include <array>
#include "Definitions.h"
#include "Grid.h"

namespace Simulation {

    class SecurityMap {
    public:
        using SArray = std::array<std::array<float, Definitions::GRID_SIZE>, Definitions::GRID_SIZE>;

        SecurityMap();

        void clear();
        void fill(float v);

        void RebuildSecurityMap(const Models::Grid& grid);

        void add(int r, int c, float v);
        float at(int r, int c) const;
        int   size() const { return Definitions::GRID_SIZE; }

        float maxValue() const;

        const SArray& data() const { return smap_; }
        SArray& data() { return smap_; }

    private:
        void traceRay(const Models::Grid& grid,
            float r0, float c0,        
            float dr, float dc,        
            float power,              
            int   maxSteps);           

    private:
        SArray smap_{};
    };

} // namespace Sim
