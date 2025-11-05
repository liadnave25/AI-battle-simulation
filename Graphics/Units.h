#pragma once
#include "Definitions.h"
#include "AIEvents.h"   
#include "EventBus.h"  
#include "Orders.h"     
#include "Pathfinding.h" 

namespace AI { class StateMachine; }

namespace Models {

    struct Stats {
        int hp = Definitions::HP_MAX;
        int ammo = Definitions::AMMO_INIT;
        int grenades = Definitions::GRENADE_INIT;
    };

    class Unit {
    public:
        Definitions::Team team;
        Definitions::Role role;
        int  row, col;
        bool isAlive, isMoving;
        Stats stats;
        int id;
        bool isCarryingObjective;
        bool isFighting;
        bool isInCover;
        bool isAutonomous;

        union RoleSpecificData {
            int currentHealPool; // For Medic
            int currentAmmo;     // For Supplier

            RoleSpecificData() : currentHealPool(0) {}
        } roleData;

        AI::StateMachine* m_fsm;
        AI::Pathfinding::Path m_currentPath;

        int assignedSupplyTargetId = -1;
        int assignedHealTargetId = -1;
        int supportLockUntilFrame = 0;



        Unit(Definitions::Team t, Definitions::Role r, int r0, int c0);
        virtual ~Unit();
        virtual char roleLetter() const;

        inline void report(AI::EventType type, int r = -1, int c = -1, int extra = 0) const {
            AI::Message msg{ type, id, -1, r, c, extra };
            AI::EventBus::instance().publish(msg);
        }
        inline void receiveOrder(const AI::Order& o) {
            AI::Message ack{ AI::EventType::OrderAck, id, -1 };
            AI::EventBus::instance().publish(ack);
        }
        inline float hpNorm() const {
            if (Definitions::HP_MAX <= 0) return 0.0f;
            float x = static_cast<float>(stats.hp) / static_cast<float>(Definitions::HP_MAX);
            if (x < 0.f) x = 0.f; if (x > 1.f) x = 1.f; return x;
        }
    };

} // namespace Models
