#pragma once
#include "Units.h"

namespace AI {

    class State;

    class StateMachine {
    private:
        Models::Unit* m_owner;
        State* m_currentState;

        static inline bool isPoisonPtr(const void* p) {
#if defined(_MSC_VER)
            const uintptr_t v = reinterpret_cast<uintptr_t>(p);
            return v == 0xDDDDDDDDu || v == 0xFEEEFEEEu;
#else
            (void)p; return false;
#endif
        }

    public:
        explicit StateMachine(Models::Unit* owner);
        ~StateMachine();

        void Init(State* initialState);
        void Update();
        void ChangeState(State* newState);

        State* GetCurrentState() const {
            return (m_currentState && !isPoisonPtr(m_currentState)) ? m_currentState : nullptr;
        }
    };

} // namespace AI