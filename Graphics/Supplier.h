#pragma once
#include "Units.h"
#include <cstdio>

namespace Models {

    class Supplier : public Unit {
    public:

        Supplier(Definitions::Team t, int r0, int c0)
            : Unit(t, Definitions::Role::Supplier, r0, c0)
        {
            printf("DEBUG: Supplier Unit %d created. Initial currentAmmo = %d\n", id, roleData.currentAmmo);
        }

        char roleLetter() const override { return 'P'; }
    };

} // namespace Models