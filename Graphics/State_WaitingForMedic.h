#pragma once
#include "State.h"

namespace AI {

    class State_WaitingForMedic : public State {
    public:
        State_WaitingForMedic() {}
        virtual ~State_WaitingForMedic() {}

        virtual void Enter(Models::Unit* unit) override;
        virtual void Update(Models::Unit* unit) override;
        virtual void Exit(Models::Unit* unit) override;

        // This unit is waiting, it cannot report other things
        virtual bool CanReport() const override { return false; }
    };

} // namespace AI