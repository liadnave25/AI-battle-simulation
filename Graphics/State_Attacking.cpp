#include "State_Attacking.h"
#include "State_MovingToTarget.h"
#include "Units.h"
#include "StateMachine.h"
#include "Visibility.h"
#include "Pathfinding.h"
#include "Combat.h"
#include "Globals.h"
#include "State_Idle.h"
#include "State_RetreatingToCover.h"

#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdio>

namespace {

    constexpr int kGrenadeThrowRangeCells = 7;
    constexpr int kGrenadeBlastRadius = 2;
    constexpr int kGrenadeDamage = 45;
    constexpr int kMinEnemiesForGrenade = 2;
    constexpr int kGrenadeCooldownFrames = 25;

}

namespace AI {

    static inline bool enemyAtCell(const Models::Unit& me, int r, int c) {
        for (const auto* u : g_units) {
            if (!u->isAlive || u->team == me.team) continue;
            if (u->row == r && u->col == c) return true;
        }
        return false;
    }

    static bool acquireVisibleEnemy(const Models::Unit& me, int& outR, int& outC) {
        const int R2 = Definitions::SIGHT_RANGE * Definitions::SIGHT_RANGE;
        float bestD2 = std::numeric_limits<float>::infinity();
        int br = -1, bc = -1;
        for (const auto* other : g_units) {
            if (!other->isAlive || other->team == me.team) continue;
            int dr = other->row - me.row;
            int dc = other->col - me.col;
            float d2 = float(dr * dr + dc * dc);
            if (d2 > R2 || d2 >= bestD2) continue;
            if (!AI::Visibility::HasLineOfSight(g_grid, me.row, me.col, other->row, other->col)) continue;
            bestD2 = d2; br = other->row; bc = other->col;
        }
        if (br != -1) { outR = br; outC = bc; return true; }
        return false;
    }

    State_Attacking::State_Attacking(int targetR, int targetC, Combat::System* combatSys, int attackRange, int cooldown)
        : m_targetR(targetR)
        , m_targetC(targetC)
        , m_combatSystem(combatSys)
        , m_attackRangeCells(attackRange > 0 ? attackRange : Definitions::FIRE_RANGE)
        , m_attackCooldownFrames(cooldown > 0 ? cooldown : 15)
        , m_cooldownTimer(0)
    {
        if (!m_combatSystem) {
            std::printf("Warning: State_Attacking created without valid Combat::System pointer!\n");
        }
    }

    void State_Attacking::Enter(Models::Unit* unit) {
        if (!unit) return;
        m_cooldownTimer = 0;
        unit->isMoving = false;

        int vr = -1, vc = -1;
        if (acquireVisibleEnemy(*unit, vr, vc)) {
            m_targetR = vr; m_targetC = vc;
        }
    }

    static bool FriendlyInLine(const Models::Unit* shooter, int r1, int c1)
    {
        const int r0 = shooter->row, c0 = shooter->col;

        int dr = std::abs(r1 - r0);
        int dc = std::abs(c1 - c0);
        int sr = (r0 < r1) ? 1 : -1;
        int sc = (c0 < c1) ? 1 : -1;
        int err = dr - dc;

        int r = r0, c = c0;
        while (true) {
            if (!((r == r0 && c == c0) || (r == r1 && c == c1))) {
                for (const auto* u : g_units) {
                    if (!u->isAlive) continue;
                    if (u->team != shooter->team) continue;
                    if (u->row == r && u->col == c) {
                        return true;
                    }
                }
            }

            if (r == r1 && c == c1) break;

            int e2 = 2 * err;
            if (e2 > -dc) { err -= dc; r += sr; }
            if (e2 < dr) { err += dr; c += sc; }
        }
        return false;
    }

    bool State_Attacking::CanShoot(Models::Unit* unit)
    {
        if (!unit || !unit->isAlive) return false;
        if (m_targetR < 0 || m_targetC < 0) return false;

        if (unit->stats.ammo <= 0) return false;

        Models::Unit* targetPtr = nullptr;
        for (auto* u : g_units) {
            if (u->isAlive && u->row == m_targetR && u->col == m_targetC) {
                targetPtr = u;
                break;
            }
        }
        if (!targetPtr) return false;
        if (targetPtr->team == unit->team) return false;

        const int dr = m_targetR - unit->row;
        const int dc = m_targetC - unit->col;
        const int dist2 = dr * dr + dc * dc;
        const int range2 = m_attackRangeCells * m_attackRangeCells;
        if (dist2 > range2) return false;

        if (!AI::Visibility::HasLineOfSight(g_grid, unit->row, unit->col, m_targetR, m_targetC))
            return false;

        if (FriendlyInLine(unit, m_targetR, m_targetC))
            return false;

        return true;
    }

    bool State_Attacking::ShouldThrowGrenade(const Models::Unit* self) const {
        if (!self || !self->isAlive) return false;
        if (self->stats.grenades <= 0) return false;

        const int dr = m_targetR - self->row;
        const int dc = m_targetC - self->col;
        const int d2 = dr * dr + dc * dc;
        if (d2 > kGrenadeThrowRangeCells * kGrenadeThrowRangeCells) return false;

        const bool hasLOS = AI::Visibility::HasLineOfSight(g_grid, self->row, self->col, m_targetR, m_targetC);
        if (!hasLOS) return true;

        const int nearby = CountEnemiesNear(m_targetR, m_targetC, kGrenadeBlastRadius, self->team);
        return (nearby >= kMinEnemiesForGrenade);
    }

    int State_Attacking::CountEnemiesNear(int r, int c, int radius, Definitions::Team myTeam) const {
        int count = 0;
        const int rmin = std::max(0, r - radius);
        const int rmax = std::min(Definitions::GRID_SIZE - 1, r + radius);
        const int cmin = std::max(0, c - radius);
        const int cmax = std::min(Definitions::GRID_SIZE - 1, c + radius);

        for (const auto* other : g_units) {
            if (!other->isAlive || other->team == myTeam) continue;
            if (other->row < rmin || other->row > rmax || other->col < cmin || other->col > cmax) continue;
            const int dr = other->row - r;
            const int dc = other->col - c;
            if (dr * dr + dc * dc <= radius * radius) ++count;
        }
        return count;
    }

    void State_Attacking::DoThrowGrenade(Models::Unit* self) {
        if (!self || self->stats.grenades <= 0) return;

        std::printf("Unit %d throws GRENADE to (%d,%d)\n", self->id, m_targetR, m_targetC);

        for (auto* other : g_units) {
            if (!other->isAlive || other->team == self->team) continue;

            const int dr = other->row - m_targetR;
            const int dc = other->col - m_targetC;
            if (dr * dr + dc * dc <= kGrenadeBlastRadius * kGrenadeBlastRadius) {
                other->stats.hp -= kGrenadeDamage;
                if (other->stats.hp <= 0) {
                    other->stats.hp = 0;
                    other->isAlive = false;
                    std::printf("Unit %d killed by grenade.\n", other->id);
                }
                else {
                    std::printf("Unit %d damaged by grenade. HP now %d\n", other->id, other->stats.hp);
                }
            }
        }

        self->stats.grenades = std::max(0, self->stats.grenades - 1);
        m_cooldownTimer = std::max(m_cooldownTimer, kGrenadeCooldownFrames);
    }

    void State_Attacking::Update(Models::Unit* unit) {
        if (!unit || !unit->isAlive || !unit->m_fsm || !m_combatSystem) return;

        int vr = -1, vc = -1;
        if (acquireVisibleEnemy(*unit, vr, vc)) {
            m_targetR = vr; m_targetC = vc;
        }

        if (m_cooldownTimer > 0) {
            m_cooldownTimer--;
        }

        if (ShouldThrowGrenade(unit) && m_cooldownTimer == 0) {
            DoThrowGrenade(unit);
        }

        if (CanShoot(unit)) {
            unit->isMoving = false;
            if (m_cooldownTimer == 0) {
                if (unit->stats.ammo > 0) {
                    m_combatSystem->fireBulletTowards(
                        (float)unit->row, (float)unit->col,
                        (float)m_targetR, (float)m_targetC,
                        unit->team
                    );
                    unit->stats.ammo--;
                    m_cooldownTimer = m_attackCooldownFrames;
                    std::printf("Unit %d fired. Ammo left: %d\n", unit->id, unit->stats.ammo);
                }
                else {
                    std::printf("Unit %d (Attacking): Out of ammo!\n", unit->id);
                    AI::EventBus::instance().publish(AI::Message{
                        AI::EventType::LowAmmo, unit->id, -1, unit->row, unit->col, unit->stats.ammo
                        });
                    unit->m_fsm->ChangeState(new State_RetreatingToCover());
                }
            }
            return;
        }

        AI::Pathfinding::Cell destination = { -1, -1 };
        AI::Pathfinding::Cell vantagePoint = AI::Pathfinding::PickVantagePoint(
            g_grid, g_smap,
            { unit->row, unit->col },
            { m_targetR, m_targetC },
            m_attackRangeCells,
            0.15f,
            10
        );

        if (vantagePoint.first != -1) destination = vantagePoint;
        else                          destination = { m_targetR, m_targetC };

        State* nextStateOnArrival = new State_Attacking(
            m_targetR, m_targetC, m_combatSystem, m_attackRangeCells, m_attackCooldownFrames
        );

        AI::Pathfinding::Path potentialPath = AI::Pathfinding::AStar_FindPath(
            unit,
            g_grid, g_smap,
            { unit->row, unit->col },
            destination,
            5.5f
        );

        if (potentialPath.empty()) {
            std::printf("Unit %d (Attacking): No path to (%d,%d). Switching to Idle.\n",
                unit->id, destination.first, destination.second);
            delete nextStateOnArrival;
            nextStateOnArrival = nullptr;

            unit->m_fsm->ChangeState(new State_Idle());
        }
        else {
            unit->m_fsm->ChangeState(
                new State_MovingToTarget(destination.first, destination.second, nextStateOnArrival)
            );
        }
    }

    void State_Attacking::Exit(Models::Unit* unit) {
        if (!unit) return;
        unit->isMoving = false;
        unit->m_currentPath.clear();
    }

} // namespace AI
