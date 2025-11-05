#pragma once

namespace Models {
    class Unit; 
}

namespace AI {

    class State {
    public:
        virtual ~State() {}
        virtual void Enter(Models::Unit* unit) = 0;
        virtual void Update(Models::Unit* unit) = 0;
        virtual void Exit(Models::Unit* unit) = 0;

        virtual bool CanReport() const { return true; }
    };
} // namespace AI