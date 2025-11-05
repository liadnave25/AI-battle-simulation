#include "State_MovingToTarget.h"
#include "State_Idle.h"
#include "Units.h"
#include "Globals.h"              
#include "Pathfinding.h"          
#include "StateMachine.h"
#include "Definitions.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

namespace AI {

    static void ReplanAStar(Models::Unit* unit, int targetR, int targetC) {
        unit->m_currentPath = AI::Pathfinding::AStar_FindPath(
            unit,
            g_grid,
            g_smap,
            { unit->row, unit->col },
            { targetR, targetC },
            AI::Pathfinding::RiskWeightForUnit(unit)
        );
    }

    static inline bool IsLegalStep(int r, int c) {
        if (r < 0 || r >= Definitions::GRID_SIZE || c < 0 || c >= Definitions::GRID_SIZE)
            return false;
        return AI::Pathfinding::IsWalkableForMovement(g_grid.at(r, c));
    }


    State_MovingToTarget::State_MovingToTarget(int r, int c, State* onArrivalState, int framesPerMove)
        : m_targetR(r),
        m_targetC(c),
        m_onArrivalState(onArrivalState),
        m_framesPerStep(std::max(1, framesPerMove)),
        m_stepCounter(0)
    {
        if (!m_onArrivalState) {
            m_onArrivalState = new State_Idle();
        }
    }

    State_MovingToTarget::~State_MovingToTarget()
    {
        if (m_onArrivalState) {
            delete m_onArrivalState;
            m_onArrivalState = nullptr;
        }
    }

    void State_MovingToTarget::Enter(Models::Unit* unit)
    {
        if (!unit) return;

        unit->isMoving = true;
        m_stepCounter = 0;
        unit->m_currentPath.clear();

        ReplanAStar(unit, m_targetR, m_targetC);

        if (unit->m_currentPath.empty()) {
            printf("Unit %d: No path found to target (%d,%d) in Enter. Switching to Idle.\n", unit->id, m_targetR, m_targetC);
            if (m_onArrivalState) {
                delete m_onArrivalState;
                m_onArrivalState = nullptr;
            }
            unit->m_fsm->ChangeState(new State_Idle());
            return;
        }
    }

    void State_MovingToTarget::Update(Models::Unit* unit)
    {
        if (!unit || !unit->m_fsm) return;

        if (unit->role == Definitions::Role::Supplier) {
            static int supplierMoveCounter = 0;
            if (supplierMoveCounter++ % 30 == 0) {
                printf("DEBUG: Unit %d (Supplier) in MovingToTarget::Update. currentAmmo = %d\n", unit->id, unit->roleData.currentAmmo);
            }
        }

        if (unit->m_currentPath.size() <= 1) {
            State* nextState = m_onArrivalState;
            m_onArrivalState = nullptr;
            unit->m_fsm->ChangeState(nextState);
            return;
        }

        m_stepCounter++;
        if (m_stepCounter < m_framesPerStep) {
            return;
        }
        m_stepCounter = 0;

        auto& nextStep = unit->m_currentPath[1];
        int nextR = nextStep.first;
        int nextC = nextStep.second;

        bool needsReplan = false;
        const char* replanReason = "";

        if (!IsLegalStep(nextR, nextC)) {
            needsReplan = true;
            replanReason = "Illegal Step";
        }
        else if (AI::Pathfinding::IsOccupied(nextR, nextC)) {
            if (nextR == m_targetR && nextC == m_targetC) {
                int dist = std::abs(unit->row - m_targetR) + std::abs(unit->col - m_targetC);
                if (dist == 1) {
                    printf("Unit %d: Arrived adjacent to occupied target (%d,%d). Switching state.\n", unit->id, m_targetR, m_targetC);
                    State* nextState = m_onArrivalState;
                    m_onArrivalState = nullptr;
                    unit->m_fsm->ChangeState(nextState);
                    return;
                }
            }
            needsReplan = true;
            replanReason = "Occupied";
        }
        else
        {
            constexpr int SAMPLE_LEN = 8;
            float riskMaxNorm = AI::Pathfinding::PathRiskSample(
                g_smap, unit->m_currentPath, SAMPLE_LEN, true);

            if (riskMaxNorm >= Definitions::REPLAN_RISK_DELTA) {
                needsReplan = true;
                replanReason = "High Risk";
            }
        }

        if (needsReplan) {
            ReplanAStar(unit, m_targetR, m_targetC);

            if (unit->m_currentPath.empty()) {
                printf("Unit %d: No path found to target (%d,%d) after replan [%s]. Switching to Idle.\n", unit->id, m_targetR, m_targetC, replanReason);
                if (m_onArrivalState) { delete m_onArrivalState; m_onArrivalState = nullptr; }
                unit->m_fsm->ChangeState(new State_Idle());
                return;
            }

            if (unit->m_currentPath.size() <= 1) {
                return;
            }

            nextR = unit->m_currentPath[1].first;
            nextC = unit->m_currentPath[1].second;

            if (!IsLegalStep(nextR, nextC) || AI::Pathfinding::IsOccupied(nextR, nextC)) {
                return;
            }
        }

        unit->row = nextR;
        unit->col = nextC;

        if (!unit->m_currentPath.empty()) {
            unit->m_currentPath.erase(unit->m_currentPath.begin());
        }
    }


    void State_MovingToTarget::Exit(Models::Unit* unit)
    {
        if (!unit) return;
        unit->isMoving = false;
        unit->m_currentPath.clear();
    }

} // namespace AI
