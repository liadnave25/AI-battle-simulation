#pragma once
#include "State.h"
#include "Definitions.h"
#include "Pathfinding.h"

namespace AI {

    class State_Healing : public State {
    private:
        int m_targetUnitId = -1;

        struct Navigator {
            std::vector<AI::Pathfinding::Cell> path;
            size_t i = 0;
            void reset() { path.clear(); i = 0; }
            bool step(Models::Unit* self, int goalR, int goalC);
        } nav;

        bool healOnce(Models::Unit* self, Models::Unit* tgt);

    public:
        explicit State_Healing(int targetId);
        ~State_Healing() override = default;

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit) override;
        bool CanReport() const override { return true; }
    };

} // namespace AI