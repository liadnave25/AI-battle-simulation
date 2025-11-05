#include "State_Idle.h"
#include "Definitions.h"
#include "Units.h"
#include "Globals.h"
#include "StateMachine.h"
#include "Pathfinding.h"
#include "Visibility.h"
#include "Combat.h"

#include "State_Attacking.h"
#include "State_RetreatingToCover.h"
#include "State_Supplying.h"
#include "State_RefillAtDepot.h"
#include "State_Healing.h"

#include <limits>
#include <cmath>
#include <cstdio>
#include <functional> 

namespace AI {

    static int findClosestTeammateInNeed(const Models::Unit* self,
        const std::function<bool(const Models::Unit*)>& criteria)
    {
        int bestTargetId = -1;
        int bestDist2 = std::numeric_limits<int>::max();

        for (auto* u : g_units) {
            if (!u || !u->isAlive) continue;
            if (u->team != self->team) continue;
            if (u->id == self->id) continue;

            if (criteria(u)) {
                int dr = u->row - self->row;
                int dc = u->col - self->col;
                int d2 = dr * dr + dc * dc;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    bestTargetId = u->id;
                }
            }
        }
        return bestTargetId;
    }



    State_Idle::State_Idle() {}
    State_Idle::~State_Idle() {}

    void State_Idle::Enter(Models::Unit* unit) {
        if (unit) unit->isMoving = false;
    }

    void State_Idle::Exit(Models::Unit*) {
        // nothing
    }

    void State_Idle::Update(Models::Unit* unit)
    {
        if (!unit || !unit->isAlive || !unit->m_fsm) return;

        if (unit->role == Definitions::Role::Warrior && unit->isAutonomous)
        {
            if (unit->hpNorm() * 100.0f < Definitions::HP_CRITICAL) {
                std::printf("[Warrior %d] Autonomous: Critically injured! Retreating!\n", unit->id);
                unit->m_fsm->ChangeState(new State_RetreatingToCover());
                return;
            }

            int   nearest_er = -1, nearest_ec = -1;
            float bestScore = std::numeric_limits<float>::infinity();
            const int SIGHT_RANGE_SQ = Definitions::SIGHT_RANGE * Definitions::SIGHT_RANGE;

            for (const auto* other : g_units) {
                if (!other || !other->isAlive || other->team == unit->team) continue;
                int dr = other->row - unit->row;
                int dc = other->col - unit->col;
                float d2 = float(dr * dr + dc * dc);
                if (d2 >= SIGHT_RANGE_SQ) continue;

                if (AI::Visibility::HasLineOfSight(g_grid, unit->row, unit->col, other->row, other->col)) {
                    float score = d2;
                    int enemyCell = g_grid.at(other->row, other->col);
                    if (enemyCell == Definitions::Cell::TREE) {
                        score *= Definitions::TREE_COVER_PENALTY_MULTIPLIER;
                    }
                    if (score < bestScore) {
                        bestScore = score;
                        nearest_er = other->row;
                        nearest_ec = other->col;
                    }
                }
            }

            if (nearest_er != -1) {
                std::printf("[Warrior %d] Autonomous: Target acquired! Attacking!\n", unit->id);
                unit->m_fsm->ChangeState(new State_Attacking(nearest_er, nearest_ec, &g_combat));
                return;
            }
            return;
        }

        if (unit->role == Definitions::Role::Supplier && unit->isAutonomous)
        {
            auto needsAmmo = [](const Models::Unit* u) {
                return (u->role == Definitions::Role::Warrior && u->stats.ammo <= 0);
                };

            int bestTargetId = findClosestTeammateInNeed(unit, needsAmmo);

            if (bestTargetId != -1) {
                if (unit->roleData.currentAmmo < Definitions::SUPPLIER_REFILL_THRESHOLD) {
                    unit->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Supplier, bestTargetId));
                }
                else {
                    unit->m_fsm->ChangeState(new State_Supplying(bestTargetId));
                }
                return;
            }
            return;
        }

        if (unit->role == Definitions::Role::Medic && unit->isAutonomous)
        {
            auto isInjured = [](const Models::Unit* u) {
                return (u->stats.hp < Definitions::HP_MAX);
                };

            int bestTargetId = findClosestTeammateInNeed(unit, isInjured);

            if (bestTargetId != -1) {
                if (unit->roleData.currentHealPool < Definitions::MEDIC_REFILL_THRESHOLD) {
                    unit->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Medic, bestTargetId));
                }
                else {
                    unit->m_fsm->ChangeState(new State_Healing(bestTargetId));
                }
                return;
            }
            return;
        }
    }

} // namespace AI
