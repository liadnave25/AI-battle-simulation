#pragma once
#include "State.h"
#include "Definitions.h"
#include "Combat.h"

namespace Models { class Unit; }

namespace AI {

    class State_Attacking : public State {
    private:
        int m_targetR;
        int m_targetC;

        int m_attackRangeCells;
        int m_attackCooldownFrames;
        int m_cooldownTimer;
        Combat::System* m_combatSystem;

    public:
        State_Attacking(int targetR, int targetC, Combat::System* combatSys,
            int attackRange = Definitions::FIRE_RANGE,
            int cooldown = 15);
        virtual ~State_Attacking() {}

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit)   override;

    private:
        bool CanShoot(Models::Unit* unit);

        bool ShouldThrowGrenade(const Models::Unit* self) const;
        int  CountEnemiesNear(int r, int c, int radius, Definitions::Team myTeam) const;
        void DoThrowGrenade(Models::Unit* self);
    };

} // namespace AI