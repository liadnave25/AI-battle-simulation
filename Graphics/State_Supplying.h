#pragma once
#include "State.h"
#include "Definitions.h"
#include "Pathfinding.h"

namespace AI {

    class State_Supplying : public State {
    private:
        int m_targetUnitId = -1;

        struct Navigator {
            AI::Pathfinding::Path path;
            size_t i = 0;

            void reset() { path.clear(); i = 0; }
            bool step(Models::Unit* self, int goalR, int goalC);
        } nav;

        bool supplyOnce(Models::Unit* self, Models::Unit* tgt);

    public:
        explicit State_Supplying(int targetId);
        ~State_Supplying() override = default;

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit) override;
        bool CanReport() const override { return true; }
    };

} // namespace AI