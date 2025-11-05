#pragma once
#include "Units.h"

namespace Combat { struct System; }

namespace Models {

    class Warrior : public Unit {
    public:
        Warrior(Definitions::Team t, int r0, int c0);

        virtual char roleLetter() const override;

        void CheckAndReportStatus();

        bool TryThrowGrenade(int targetRow, int targetCol);

    private:
        bool m_reportedInjured = false;
        bool m_reportedLowAmmo = false;
        bool m_isCriticallyInjured = false;
        bool m_reportedUnderFire = false;
        int  m_sightCooldown = 0;

        void ConsiderAutoGrenade();
        int  m_grenadeCooldownTicks = 0;

        bool acquireVisibleEnemy(int& outR, int& outC) const;
    };

} // namespace Models
