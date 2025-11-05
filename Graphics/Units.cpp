#include "Units.h"
#include "StateMachine.h"
#include "State_Idle.h"
#include "State.h"
#include "Definitions.h"
#include "EventBus.h"
#include "AIEvents.h"
#include "Globals.h"
#include "Pathfinding.h"
#include "State_Healing.h"
#include "State_Supplying.h"
#include "State_RefillAtDepot.h"
#include "Visibility.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <limits>

using AI::EventBus;
using AI::Message;
using AI::EventType;

namespace {

    inline Models::Unit* findUnitById(int uid) {
        if (uid <= 0) return nullptr;
        for (auto* u : g_units) if (u && u->id == uid) return u;
        return nullptr;
    }

    inline bool isInterruptible(Models::Unit* u) {
        if (!u || !u->isAlive || !u->m_fsm) return false;
        AI::State* s = u->m_fsm->GetCurrentState();
        return (s && s->CanReport());
    }

}

namespace Models {

    Unit::Unit(Definitions::Team t, Definitions::Role r, int r0, int c0)
        : team(t), role(r), row(r0), col(c0),
        isAlive(true), isMoving(false),
        stats{}, id(-1), isCarryingObjective(false),
        isFighting(false), isInCover(false), isAutonomous(false),
        roleData(),
        m_fsm(nullptr), m_currentPath(),
        assignedSupplyTargetId(-1),
        assignedHealTargetId(-1),
        supportLockUntilFrame(0)
    {
        m_fsm = new AI::StateMachine(this);
        m_fsm->Init(new AI::State_Idle());

        if (role == Definitions::Role::Medic) {
            roleData.currentHealPool = 120;
        }
        else if (role == Definitions::Role::Supplier) {
            roleData.currentAmmo = 20;
            std::printf("DEBUG: Supplier Unit %d created. Initial currentAmmo = %d\n", id, roleData.currentAmmo);
        }


        EventBus::instance().subscribe(-1, [this](const Message& m) {
            if (m.type != EventType::CommanderDown) return;

            Models::Unit* commander = findUnitById(m.fromUnitId);
            if (!commander || commander->team != this->team) return;

            if (!this->isAutonomous) {
                this->isAutonomous = true;
                std::printf("[Unit %d] CommanderDown for team -> switching to AUTONOMOUS\n", this->id);

                if (this->m_fsm && isInterruptible(this)) {
                    this->m_fsm->ChangeState(new AI::State_Idle());
                }
            }
            });

        if (role == Definitions::Role::Medic) {
            EventBus::instance().subscribe(-1, [this](const Message& m) {
                if (!this->isAutonomous) return;
                if (m.type != EventType::Injured) return;
                if (!this->isAlive || !this->m_fsm) return;

                Models::Unit* inj = findUnitById(m.fromUnitId);
                if (!inj || !inj->isAlive) return;
                if (inj->team != this->team) return;
                if (!isInterruptible(this)) return;

                if (this->roleData.currentHealPool < Definitions::MEDIC_REFILL_THRESHOLD) {
                    this->m_fsm->ChangeState(new AI::State_RefillAtDepot(Definitions::Role::Medic, inj->id));
                    std::printf("[Medic %d] Autonomous: REFILL then HEAL Unit %d\n", this->id, inj->id);
                }
                else {
                    this->m_fsm->ChangeState(new AI::State_Healing(inj->id));
                    std::printf("[Medic %d] Autonomous: HEAL Unit %d\n", this->id, inj->id);
                }
                this->assignedHealTargetId = inj->id;
                });
        }

        if (role == Definitions::Role::Supplier) {
            EventBus::instance().subscribe(-1, [this](const Message& m) {
                if (!this->isAutonomous) return;
                if (m.type != EventType::LowAmmo) return;
                if (!this->isAlive || !this->m_fsm) return;

                Models::Unit* needy = findUnitById(m.fromUnitId);
                if (!needy || !needy->isAlive) return;
                if (needy->team != this->team) return;
                if (!isInterruptible(this)) return;

                if (this->roleData.currentAmmo < Definitions::SUPPLIER_REFILL_THRESHOLD) {
                    this->m_fsm->ChangeState(new AI::State_RefillAtDepot(Definitions::Role::Supplier, needy->id));
                    std::printf("[Supplier %d] Autonomous: REFILL then SUPPLY Unit %d\n", this->id, needy->id);
                }
                else {
                    this->m_fsm->ChangeState(new AI::State_Supplying(needy->id));
                    std::printf("[Supplier %d] Autonomous: SUPPLY Unit %d\n", this->id, needy->id);
                }
                this->assignedSupplyTargetId = needy->id;
                });
        }
    }

    Unit::~Unit()
    {
        if (m_fsm) {
            delete m_fsm;
            m_fsm = nullptr;
        }
    }

    char Unit::roleLetter() const {
        using Definitions::Role;
        switch (role) {
        case Role::Commander: return 'C';
        case Role::Warrior:   return 'W';
        case Role::Medic:     return 'M';
        case Role::Supplier:  return 'P';
        }
        return '?';
    }

} // namespace Models
