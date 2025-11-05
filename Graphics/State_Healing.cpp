#include "State_Healing.h"
#include "State_RefillAtDepot.h"
#include "State_Idle.h"
#include "StateMachine.h"
#include "Units.h"
#include "Definitions.h"
#include "Globals.h"
#include "Pathfinding.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
    inline Models::Unit* findUnitById(int uid) {
        if (uid <= 0) return nullptr;
        for (auto& u : g_units) if (u->id == uid) return u;
        return nullptr;
    }
    inline int manhattan(int r1, int c1, int r2, int c2) { return std::abs(r1 - r2) + std::abs(c1 - c2); }
}

namespace AI {

    State_Healing::State_Healing(int targetId)
        : m_targetUnitId(targetId) {
    }

    bool State_Healing::Navigator::step(Models::Unit* self, int goalR, int goalC) {
        if (!self) return false;

        if (path.empty() || i >= path.size()) {
            path = AI::Pathfinding::AStar_FindPath(
                self, g_grid, g_smap,
                { self->row, self->col },
                { goalR, goalC },
                AI::Pathfinding::RiskWeightForUnit(self)
            );
            i = 0;
            if (path.empty()) return false;
        }

        const auto next = path[i++];
        self->row = next.first;
        self->col = next.second;
        return true;
    }

    bool State_Healing::healOnce(Models::Unit* self, Models::Unit* tgt) {
        if (!self || !tgt) return false;

        const int need = std::max(0, Definitions::HP_MAX - tgt->stats.hp);
        if (need == 0) return false;

        const int perTick = Definitions::MEDIC_HEAL_AMOUNT;
        const int canGive = std::min({ perTick, need, self->roleData.currentHealPool });
        if (canGive <= 0) return false;

        tgt->stats.hp = std::min(Definitions::HP_MAX, tgt->stats.hp + canGive);
        self->roleData.currentHealPool = std::max(0, self->roleData.currentHealPool - canGive);

        std::printf("Medic #%d -> Unit #%d : +%d HP (pool left: %d)\n",
            self->id, tgt->id, canGive, self->roleData.currentHealPool);

        return true;
    }

    void State_Healing::Enter(Models::Unit* unit) {
        if (!unit || !unit->m_fsm || unit->role != Definitions::Role::Medic) {
            if (unit && unit->m_fsm) unit->m_fsm->ChangeState(new State_Idle());
            return;
        }
        unit->isMoving = true;
        nav.reset();
    }

    void State_Healing::Update(Models::Unit* unit) {
        if (!unit || !unit->m_fsm) return;

        Models::Unit* tgt = findUnitById(m_targetUnitId);
        if (!tgt || !tgt->isAlive) {
            unit->m_fsm->ChangeState(new State_Idle());
            return;
        }

        if (manhattan(unit->row, unit->col, tgt->row, tgt->col) <= 1) {
            const bool did = healOnce(unit, tgt);
            const bool done = (!did) || (tgt->stats.hp >= Definitions::HP_MAX) || (unit->roleData.currentHealPool <= 0);
            if (done) {
                unit->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Medic, -1));
            }

            return;
        }

        if (!nav.step(unit, tgt->row, tgt->col)) {
            unit->m_fsm->ChangeState(new State_Idle());
            return;
        }
    }

    void State_Healing::Exit(Models::Unit* unit) {
        if (unit) unit->isMoving = false;
    }

} // namespace AI