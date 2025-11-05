#pragma once
#include "State.h"
#include "Pathfinding.h" 

namespace AI {

    class State_RetreatingToCover : public State {
    public:
        State_RetreatingToCover() {}
        virtual ~State_RetreatingToCover() {}

        virtual void Enter(Models::Unit* unit) override;
        virtual void Update(Models::Unit* unit) override;
        virtual void Exit(Models::Unit* unit) override;

        // This unit is busy retreating, it cannot report other things
        virtual bool CanReport() const override { return false; }
    };

} // namespace AI