#include "State_Defending.h"
#include "State_MovingToTarget.h"
#include "State_Idle.h"
#include "Units.h"
#include "StateMachine.h"
#include "Pathfinding.h"
#include "Globals.h"      
#include "Definitions.h"

#include <queue>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace AI {

    static inline bool inBounds(int r, int c) {
        return r >= 0 && r < Definitions::GRID_SIZE && c >= 0 && c < Definitions::GRID_SIZE;
    }

    static bool hasBlockingNeighbor(const Models::Grid& grid, int r, int c) {
        using namespace Definitions;
        static const int dr[4] = { +1,-1,0,0 };
        static const int dc[4] = { 0,0,+1,-1 };
        for (int k = 0; k < 4; ++k) {
            int rr = r + dr[k], cc = c + dc[k];
            if (!inBounds(rr, cc)) continue;
            int cell = grid.at(rr, cc);
            if (cell == ROCK || cell == TREE) return true;
        }
        return false;
    }

    static bool isCoverCell(const Models::Grid& grid, int r, int c, float riskNorm, float riskThreshold) {
        if (!AI::Pathfinding::IsWalkableForMovement(grid.at(r, c))) return false;
        if (AI::Pathfinding::IsOccupied(r, c)) return false;
        if (riskNorm > riskThreshold) return false;
        if (!hasBlockingNeighbor(grid, r, c)) return false;
        return true;
    }

    static bool BFS_FindCoverAround(const Models::Grid& grid,
        const Simulation::SecurityMap& smap,
        int anchorR, int anchorC,
        int radius,
        float riskThreshold,
        int& outR, int& outC)
    {
        using namespace Definitions;
        outR = outC = -1;
        if (!inBounds(anchorR, anchorC)) return false;
        const float maxRisk = std::max(0.001f, smap.maxValue());
        const int N = GRID_SIZE;
        static std::vector<uint8_t> visited;
        visited.assign(N * N, 0);
        auto idx = [N](int r, int c) { return r * N + c; };
        struct Node { int r, c, d; };
        std::queue<Node> q;
        q.push({ anchorR, anchorC, 0 });
        visited[idx(anchorR, anchorC)] = 1;
        const int dr[4] = { +1, -1, 0, 0 };
        const int dc[4] = { 0, 0, +1, -1 };
        {
            float riskNorm = smap.at(anchorR, anchorC) / maxRisk;
            if (isCoverCell(grid, anchorR, anchorC, riskNorm, riskThreshold)) {
                outR = anchorR; outC = anchorC;
                return true;
            }
        }
        while (!q.empty()) {
            Node cur = q.front(); q.pop();
            if (cur.d >= radius) continue;
            for (int k = 0; k < 4; ++k) {
                int nr = cur.r + dr[k];
                int nc = cur.c + dc[k];
                if (!inBounds(nr, nc)) continue;
                int id = idx(nr, nc);
                if (visited[id]) continue;
                visited[id] = 1;
                int manhattan = std::abs(nr - anchorR) + std::abs(nc - anchorC);
                if (manhattan > radius) continue;
                float riskNorm = smap.at(nr, nc) / maxRisk;
                if (isCoverCell(grid, nr, nc, riskNorm, riskThreshold)) {
                    outR = nr; outC = nc;
                    return true;
                }
                q.push({ nr, nc, cur.d + 1 });
            }
        }
        return false;
    }

    static void findAndMoveToBestSpot(Models::Unit* unit, int anchorR, int anchorC, int defendRadius, float riskThreshold)
    {
        if (!unit || !unit->m_fsm) return;

        int targetR = -1, targetC = -1;

        bool foundSpot = BFS_FindCoverAround(
            g_grid, g_smap,
            anchorR, anchorC,
            defendRadius,
            riskThreshold,
            targetR, targetC
        );

        if (foundSpot) {
            int manhattan = std::abs(unit->row - targetR) + std::abs(unit->col - targetC);
            if (manhattan <= 1) {
                unit->m_fsm->ChangeState(new State_Idle());
            }
            else {
                unit->m_fsm->ChangeState(new State_MovingToTarget(targetR, targetC, new State_Idle()));
            }
        }
        else {
            if (inBounds(anchorR, anchorC) &&
                AI::Pathfinding::IsWalkableForMovement(g_grid.at(anchorR, anchorC)))
            {
                unit->m_fsm->ChangeState(new State_MovingToTarget(anchorR, anchorC, new State_Idle()));
            }
            else {
                unit->m_fsm->ChangeState(new State_Idle());
            }
        }
    }


    State_Defending::State_Defending(int anchorR, int anchorC, int radius, float riskThreshold)
        : m_anchorR(anchorR),
        m_anchorC(anchorC),
        m_defendRadius(radius > 0 ? radius : 6),
        m_riskThreshold(std::max(0.0f, std::min(riskThreshold, 1.0f)))
    {
    }

    void State_Defending::Enter(Models::Unit* unit) {
        findAndMoveToBestSpot(unit, m_anchorR, m_anchorC, m_defendRadius, m_riskThreshold);
    }

    void State_Defending::Update(Models::Unit* unit) {
        findAndMoveToBestSpot(unit, m_anchorR, m_anchorC, m_defendRadius, m_riskThreshold);
    }


    void State_Defending::Exit(Models::Unit* unit) {
        if (unit) unit->isMoving = false;
    }

} // namespace AI
