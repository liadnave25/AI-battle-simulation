#pragma once
#include "State.h"

namespace Models { class Unit; }

namespace AI {

    class State_Defending : public State {
    private:
        int m_anchorR;
        int m_anchorC;
        int m_defendRadius;

        float m_riskThreshold;

    public:
        State_Defending(int anchorR, int anchorC, int radius = 6, float riskThreshold = 0.25f);
        virtual ~State_Defending() {}

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit) override;
    };

} // namespace AI