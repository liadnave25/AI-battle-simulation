#include "State_WaitingForSupport.h"
#include "Units.h"
#include "StateMachine.h" 
#include "State_Idle.h"  

#include <cstdio> 

namespace AI {

    void State_WaitingForSupport::Enter(Models::Unit* unit) {
        if (!unit) return;
        unit->isMoving = false;
        unit->isInCover = true; // The unit is now waiting in a cover position
    }

    void State_WaitingForSupport::Update(Models::Unit* unit) {
        if (unit->stats.ammo <= 0 || unit->stats.hp < Definitions::HP_MED) {
            unit->m_fsm->ChangeState(new State_WaitingForSupport());
        }
        else {
            unit->m_fsm->ChangeState(new State_Idle());
        }
    }

    void State_WaitingForSupport::Exit(Models::Unit* unit) {
        if (!unit) return;
        unit->isInCover = false;
    }

}