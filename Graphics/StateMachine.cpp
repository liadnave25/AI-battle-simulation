#include "StateMachine.h"
#include "State.h"
#include <cstdio>

namespace AI {

    StateMachine::StateMachine(Models::Unit* owner)
        : m_owner(owner), m_currentState(nullptr) {
    }

    StateMachine::~StateMachine() {
        if (m_currentState && !isPoisonPtr(m_currentState)) {
            m_currentState->Exit(m_owner);
            delete m_currentState;
        }
        m_currentState = nullptr;
        m_owner = nullptr;
    }

    void StateMachine::Init(State* initialState) {
        if (m_currentState && !isPoisonPtr(m_currentState)) {
            m_currentState->Exit(m_owner);
            delete m_currentState;
        }
        m_currentState = nullptr;

        m_currentState = initialState;
        if (m_currentState && !isPoisonPtr(m_currentState)) {
            m_currentState->Enter(m_owner);
        }
    }

    void StateMachine::Update() {
        if (!m_owner || isPoisonPtr(m_owner)) {
            std::printf("[FSM] WARNING: owner is null/poison, skipping Update.\n");
            return;
        }
        if (!m_currentState || isPoisonPtr(m_currentState)) {

            std::printf("[FSM] WARNING: current state is null/poison for Unit#%d. Skipping Update.\n",
                m_owner ? m_owner->id : -1);
            return;
        }

        m_currentState->Update(m_owner);
    }

    void StateMachine::ChangeState(State* newState) {
        if (!m_owner || isPoisonPtr(m_owner)) {
            std::printf("[FSM] WARNING: ChangeState on invalid owner. Ignored.\n");
            if (newState && !isPoisonPtr(newState)) delete newState;
            return;
        }

        if (newState == m_currentState) {
            std::printf("[FSM] NOTE: ChangeState called with same state object. Ignored.\n");
            return;
        }

        if (m_currentState && !isPoisonPtr(m_currentState)) {
            m_currentState->Exit(m_owner);
            State* old = m_currentState;
            m_currentState = nullptr;
            delete old;
        }
        else {
            m_currentState = nullptr;
        }

        m_currentState = newState;

        if (m_currentState && !isPoisonPtr(m_currentState)) {
            m_currentState->Enter(m_owner);
        }
    }

} // namespace AI
