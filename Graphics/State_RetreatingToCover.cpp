#include "State_RetreatingToCover.h"
#include "State_WaitingForSupport.h"
#include "State_MovingToTarget.h"
#include "StateMachine.h"
#include "Units.h"
#include "Pathfinding.h"
#include "Globals.h"
#include "Definitions.h"
#include "Visibility.h"

#include <vector>
#include <limits>
#include <cmath>
#include <algorithm> 

struct CoverCandidate {
    int r, c;
    float score;
};

static bool isAdjacentToRock(int r, int c) {
    const int dr[] = { -1, 1, 0, 0, -1, -1, 1, 1 };
    const int dc[] = { 0, 0, -1, 1, -1, 1, -1, 1 };

    for (int i = 0; i < 8; ++i) {
        int nr = r + dr[i];
        int nc = c + dc[i];
        if (nr >= 0 && nr < Definitions::GRID_SIZE && nc >= 0 && nc < Definitions::GRID_SIZE) {
            if (g_grid.at(nr, nc) == Definitions::ROCK) {
                return true;
            }
        }
    }
    return false;
}

static const Models::Unit* findNearestEnemy(const Models::Unit* self) {
    const Models::Unit* nearestEnemy = nullptr;
    float min_dist_sq = std::numeric_limits<float>::infinity();

    for (const auto* other : g_units) {
        if (other->isAlive && other->team != self->team) {
            float dr = other->row - self->row;
            float dc = other->col - self->col;
            float dist_sq = dr * dr + dc * dc;
            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                nearestEnemy = other;
            }
        }
    }
    return nearestEnemy;
}


namespace AI {

    void State_RetreatingToCover::Enter(Models::Unit* unit) {
        if (!unit || !unit->m_fsm) return;

        const int RADIUS = 35;
        const int sr = unit->row;
        const int sc = unit->col;

        const Models::Unit* nearestEnemy = findNearestEnemy(unit);
        const float maxRisk = std::max(0.001f, g_smap.maxValue());

        std::vector<CoverCandidate> rockCandidates;
        std::vector<CoverCandidate> safeCandidates;

        for (int r = sr - RADIUS; r <= sr + RADIUS; ++r) {
            for (int c = sc - RADIUS; c <= sc + RADIUS; ++c) {
                if (r < 0 || r >= Definitions::GRID_SIZE || c < 0 || c >= Definitions::GRID_SIZE) continue;
                if (!AI::Pathfinding::IsWalkableForMovement(g_grid.at(r, c))) continue;
                if (AI::Pathfinding::IsOccupiedByOther(r, c, unit->id)) continue;

                float riskScore = g_smap.at(r, c) / maxRisk;
                float dist = std::sqrt(std::pow(r - sr, 2) + std::pow(c - sc, 2));
                float distScore = dist / RADIUS;

                if (isAdjacentToRock(r, c)) {
                    float losPenalty = (nearestEnemy && AI::Visibility::HasLineOfSight(g_grid, r, c, nearestEnemy->row, nearestEnemy->col)) ? 1.0f : 0.0f;
                    float totalScore = (riskScore * 1.5f) + (distScore * 1.0f) + (losPenalty * 2.0f);
                    rockCandidates.push_back({ r, c, totalScore });
                }
                else {
                    float enemyDistBonus = 0.0f;
                    if (nearestEnemy) {
                        float distToEnemy = std::sqrt(std::pow(r - nearestEnemy->row, 2) + std::pow(c - nearestEnemy->col, 2));
                        enemyDistBonus = -(distToEnemy / (RADIUS * 2.0f));
                    }
                    float totalScore = (riskScore * 2.0f) + (distScore * 0.5f) + (enemyDistBonus * 1.5f);
                    safeCandidates.push_back({ r, c, totalScore });
                }
            }
        }

        std::sort(rockCandidates.begin(), rockCandidates.end(), [](const auto& a, const auto& b) { return a.score < b.score; });
        std::sort(safeCandidates.begin(), safeCandidates.end(), [](const auto& a, const auto& b) { return a.score < b.score; });

        for (const auto& candidate : rockCandidates) {
            auto path = AI::Pathfinding::AStar_FindPath(unit, g_grid, g_smap, { sr, sc }, { candidate.r, candidate.c });
            if (!path.empty()) {
                printf("Unit %d retreating to BEST REACHABLE rock cover at (%d, %d)\n", unit->id, candidate.r, candidate.c);
                unit->m_fsm->ChangeState(new State_MovingToTarget(candidate.r, candidate.c, new State_WaitingForSupport()));
                return;
            }
        }

        for (const auto& candidate : safeCandidates) {
            auto path = AI::Pathfinding::AStar_FindPath(unit, g_grid, g_smap, { sr, sc }, { candidate.r, candidate.c });
            if (!path.empty()) {
                printf("Unit %d (no rock found) retreating to SAFEST REACHABLE spot at (%d, %d)\n", unit->id, candidate.r, candidate.c);
                unit->m_fsm->ChangeState(new State_MovingToTarget(candidate.r, candidate.c, new State_WaitingForSupport()));
                return;
            }
        }

        printf("Unit %d is completely TRAPPED. No reachable cover found. Waiting for support at current location.\n", unit->id);
        unit->m_fsm->ChangeState(new State_WaitingForSupport());
    }

    void State_RetreatingToCover::Update(Models::Unit* unit) {
        if (unit && unit->m_fsm) {
            unit->m_fsm->ChangeState(new State_WaitingForSupport());
        }
    }

    void State_RetreatingToCover::Exit(Models::Unit* unit) {
        if (!unit) return;
        unit->isMoving = false;
    }

} // namespace AI
