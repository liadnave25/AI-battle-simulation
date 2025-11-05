#pragma once
#include "State.h"

namespace Models { class Unit; }

namespace AI {

    class State_Idle : public State {
    public:
        State_Idle();
        virtual ~State_Idle();

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit) override;
    };

} // namespace AI