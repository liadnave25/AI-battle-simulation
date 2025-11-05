#pragma once
#include "State.h"
#include "Definitions.h"
#include "Pathfinding.h"

namespace AI {

    class State_RefillAtDepot : public State {
    private:
        Definitions::Role m_unitRole;
        int m_targetUnitId;

        struct Navigator {
            std::vector<AI::Pathfinding::Cell> path;
            size_t i = 0;
            void reset() { path.clear(); i = 0; }
            bool step(Models::Unit* self, int goalR, int goalC);
        } nav;

        bool findDepotCell(Models::Unit* self, int& r, int& c) const;
        void doRefill(Models::Unit* self);

    public:
        State_RefillAtDepot(Definitions::Role unitRole, int targetUnitId);
        ~State_RefillAtDepot() override = default;

        void Enter(Models::Unit* unit) override;
        void Update(Models::Unit* unit) override;
        void Exit(Models::Unit* unit) override;
        bool CanReport() const override { return true; }
    };

} // namespace AI