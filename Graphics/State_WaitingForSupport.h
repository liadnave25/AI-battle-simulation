
#pragma once
#include "State.h"

namespace AI {

    class State_WaitingForSupport : public State {
    public:
        State_WaitingForSupport() {}
        virtual ~State_WaitingForSupport() {}

        virtual void Enter(Models::Unit* unit) override;
        virtual void Update(Models::Unit* unit) override;
        virtual void Exit(Models::Unit* unit) override;

        // A unit waiting for support cannot issue other reports
        virtual bool CanReport() const override { return true; }
    };

} // namespace AI