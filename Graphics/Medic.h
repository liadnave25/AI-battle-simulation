#pragma once
#include "Units.h"

namespace Models {

    class Medic : public Unit {
    public:
        

        Medic(Definitions::Team t, int r0, int c0)
            : Unit(t, Definitions::Role::Medic, r0, c0)
        {
        }

        char roleLetter() const override { return 'M'; }
    };

} // namespace Models