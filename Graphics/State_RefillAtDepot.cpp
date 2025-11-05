#include "State_RefillAtDepot.h"
#include "State_Supplying.h"
#include "State_Healing.h"
#include "State_Idle.h"
#include "StateMachine.h"
#include "Units.h"
#include "Definitions.h"
#include "Globals.h"
#include "Pathfinding.h"
#include "Grid.h"
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

    State_RefillAtDepot::State_RefillAtDepot(Definitions::Role unitRole, int targetUnitId)
        : m_unitRole(unitRole), m_targetUnitId(targetUnitId) {
    }

    bool State_RefillAtDepot::Navigator::step(Models::Unit* self, int goalR, int goalC) {
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

    bool State_RefillAtDepot::findDepotCell(Models::Unit* self, int& r, int& c) const {
        if (!self) return false;
        const auto& lm = g_grid.landmarks();
        if (m_unitRole == Definitions::Role::Medic) {
            if (self->team == Definitions::Team::Blue) { r = lm.medBlue.first;  c = lm.medBlue.second;  return true; }
            else { r = lm.medOrange.first; c = lm.medOrange.second; return true; }
        }
        else if (m_unitRole == Definitions::Role::Supplier) {
            if (self->team == Definitions::Team::Blue) { r = lm.ammoBlue.first; c = lm.ammoBlue.second; return true; }
            else { r = lm.ammoOrange.first; c = lm.ammoOrange.second; return true; }
        }
        return false;
    }

    void State_RefillAtDepot::doRefill(Models::Unit* self) {
        if (!self) return;
        if (m_unitRole == Definitions::Role::Medic) {
            int add = self->isFighting ? Definitions::MEDIC_REFILL_AMOUNT_FIGHTING
                : Definitions::MEDIC_REFILL_AMOUNT;
            self->roleData.currentHealPool += add;
            std::printf("Medic #%d: Refilled +%d (pool=%d)\n", self->id, add, self->roleData.currentHealPool);
        }
        else if (m_unitRole == Definitions::Role::Supplier) {
            int add = self->isFighting ? Definitions::SUPPLIER_REFILL_AMOUNT_FIGHTING
                : Definitions::SUPPLIER_REFILL_AMOUNT;
            self->roleData.currentAmmo += add;
            std::printf("Supplier #%d: Refilled +%d (stock=%d)\n", self->id, add, self->roleData.currentAmmo);
        }
    }

    void State_RefillAtDepot::Enter(Models::Unit* unit) {
        if (!unit || !unit->m_fsm) { return; }
        unit->isMoving = true;
        nav.reset();
    }

    void State_RefillAtDepot::Update(Models::Unit* unit) {
        if (!unit || !unit->m_fsm) return;

        int depotR = -1, depotC = -1;
        if (!findDepotCell(unit, depotR, depotC)) {
            unit->m_fsm->ChangeState(new State_Idle());
            return;
        }

        if (manhattan(unit->row, unit->col, depotR, depotC) <= 0) {
            doRefill(unit);

            if (m_targetUnitId > 0) {
                if (m_unitRole == Definitions::Role::Supplier) {
                    unit->m_fsm->ChangeState(new State_Supplying(m_targetUnitId));
                }
                else if (m_unitRole == Definitions::Role::Medic) {
                    unit->m_fsm->ChangeState(new State_Healing(m_targetUnitId));
                }
                else {
                    unit->m_fsm->ChangeState(new State_Idle());
                }
            }
            else {
                unit->m_fsm->ChangeState(new State_Idle());
            }
            return;
        }

        if (!nav.step(unit, depotR, depotC)) {
            unit->m_fsm->ChangeState(new State_Idle());
            return;
        }
    }

    void State_RefillAtDepot::Exit(Models::Unit* unit) {
        if (unit) unit->isMoving = false;
    }

} // namespace AI