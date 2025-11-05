#include "State_WaitingForMedic.h"
#include "Units.h"

namespace AI {

    void State_WaitingForMedic::Enter(Models::Unit* unit) {
        unit->isMoving = false;
        unit->isInCover = true;
    }

    void State_WaitingForMedic::Update(Models::Unit* unit) {
    }

    void State_WaitingForMedic::Exit(Models::Unit* unit) {
        unit->isInCover = false;
    }

} // namespace AI