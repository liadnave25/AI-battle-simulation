#pragma once
#include "State.h"
#include "Pathfinding.h"

namespace AI {

    class State_MovingToTarget : public State {
    private:
        int m_targetR;
        int m_targetC;
        State* m_onArrivalState;
        int m_framesPerStep;
        int m_stepCounter;

    public:
        State_MovingToTarget(int r, int c, State* onArrivalState, int framesPerMove = 6);
        virtual ~State_MovingToTarget();

        virtual void Enter(Models::Unit* unit) override;
        virtual void Update(Models::Unit* unit) override;
        virtual void Exit(Models::Unit* unit) override;

        virtual bool CanReport() const override { return false; }
    };

} // namespace AI