#pragma once
#include <vector>
#include <utility> 
#include <limits>  
#include "Definitions.h"
#include "Grid.h"
#include "SecurityMap.h"

namespace Models { class Unit; }

namespace AI {
    namespace Pathfinding {

        using Cell = std::pair<int, int>;
        using Path = std::vector<Cell>;

        float RiskWeightForUnit(const Models::Unit* u);

        bool PickBestDefendCell(
            int anchorR, int anchorC,
            int searchRadiusCells,
            int unitR, int unitC,
            int& outR, int& outC
        );


        bool inPlayfield(int r, int c);

        bool IsWalkableForMovement(int cell);

        Path BFS_FindPath(const Models::Grid& grid, Cell start, Cell goal);

        Path AStar_FindPath(const Models::Unit* pathingUnit,
            const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell start, Cell goal,
            float riskWeight = Definitions::ASTAR_RISK_WEIGHT);


        bool IsOccupied(int r, int c);
        bool IsOccupiedByOther(int r, int c, int pathingUnitId);


        Cell PickVantagePoint(const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell agent,
            Cell target,
            int   attackRangeCells = Definitions::FIRE_RANGE, 
            float distWeight = 0.15f,
            int   searchRadius = 10);

        Cell FindLocalCoverStep(const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell from,
            int   radius = Definitions::LOCAL_COVER_RADIUS,
            float riskDeltaMin = Definitions::LOCAL_COVER_DELTA);

        float PathRiskSample(const Simulation::SecurityMap& smap,
            const Path& path,
            int  sampleLen = 8,
            bool useMax = true);

    } // namespace Pathfinding
} // namespace AI