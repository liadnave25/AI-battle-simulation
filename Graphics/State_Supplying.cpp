#include "State_Supplying.h"
#include "State_RefillAtDepot.h"
#include "State_Idle.h"
#include "Units.h"
#include "Definitions.h"
#include "Globals.h"
#include "Pathfinding.h"
#include "StateMachine.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {
    inline Models::Unit* findUnitById(int uid) {
        if (uid <= 0) return nullptr;
        for (auto* u : g_units) if (u->id == uid) return u;
        return nullptr;
    }

    inline int cheb(int r1, int c1, int r2, int c2) {
        return std::max(std::abs(r1 - r2), std::abs(c1 - c2));
    }

    constexpr int MAX_AMMO = Definitions::AMMO_INIT;
}

namespace AI {

    State_Supplying::State_Supplying(int targetId)
        : m_targetUnitId(targetId) {
    }

    bool State_Supplying::Navigator::step(Models::Unit* self, int goalR, int goalC) {
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

    bool State_Supplying::supplyOnce(Models::Unit* supplier, Models::Unit* tgt) {
        if (!supplier || !tgt) return false;

        if (tgt->stats.ammo >= MAX_AMMO) {
            supplier->assignedSupplyTargetId = -1;
            supplier->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Supplier, -1));
            return true;
        }

        if (cheb(supplier->row, supplier->col, tgt->row, tgt->col) <= 1) {
            const int give = std::min(10, supplier->roleData.currentAmmo);
            if (give > 0) {
                supplier->roleData.currentAmmo -= give;
                tgt->stats.ammo = std::min<int>(MAX_AMMO, tgt->stats.ammo + give);

                std::printf("Supplier #%d -> Unit #%d : +%d ammo (stock left: %d)\n",
                    supplier->id, tgt->id, give, supplier->roleData.currentAmmo);
            }

            if (tgt->stats.ammo >= MAX_AMMO || supplier->roleData.currentAmmo <= 0) {
                supplier->assignedSupplyTargetId = -1;
                supplier->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Supplier, -1));
                return true;
            }
            return false;
        }

        return false;
    }

    void State_Supplying::Enter(Models::Unit* supplier) {
        if (!supplier || m_targetUnitId < 0) return;
        supplier->assignedSupplyTargetId = m_targetUnitId;

        supplier->supportLockUntilFrame = supplier->supportLockUntilFrame;
    }

    void State_Supplying::Update(Models::Unit* unit) {
        if (!unit || !unit->m_fsm) return;

        Models::Unit* tgt = findUnitById(m_targetUnitId);
        if (!tgt || !tgt->isAlive) {
            unit->assignedSupplyTargetId = -1;
            unit->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Supplier, -1));
            return;
        }

        if (cheb(unit->row, unit->col, tgt->row, tgt->col) <= 1) {
            (void)supplyOnce(unit, tgt);
            return;
        }

        if (!nav.step(unit, tgt->row, tgt->col)) {
            unit->assignedSupplyTargetId = -1;
            unit->m_fsm->ChangeState(new State_RefillAtDepot(Definitions::Role::Supplier, -1));
            return;
        }
    }

    void State_Supplying::Exit(Models::Unit* supplier) {
        if (supplier) supplier->assignedSupplyTargetId = -1;
    }

} // namespace AI