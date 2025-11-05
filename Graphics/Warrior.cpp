#include "Warrior.h"
#include "Definitions.h"
#include "AIEvents.h"
#include "EventBus.h"

#include "StateMachine.h"
#include "State.h"
#include "State_RetreatingToCover.h"
#include "State_Defending.h"
#include "State_Attacking.h"
#include "State_MovingToTarget.h"
#include "Pathfinding.h"
#include "Combat.h"

#include "Globals.h"
#include "Visibility.h"
#include "SecurityMap.h"
#include <limits> 
#include <cmath>  
#include "State_Idle.h"

extern Combat::System g_combat;

namespace {
    constexpr float GRENADE_MIN_THROW_DIST2 = 1.0f;              
    constexpr float GRENADE_MAX_THROW_DIST2 = 12.0f * 12.0f;   
    constexpr int   GRENADE_AI_COOLDOWN_TICKS = 180;            

    constexpr float DECISION_AOE_RADIUS = 2.8f;
    constexpr float DECISION_AOE_RADIUS2 = DECISION_AOE_RADIUS * DECISION_AOE_RADIUS;

    constexpr float SAFETY_FRIEND_RADIUS = 2.2f;
    constexpr float SAFETY_FRIEND_RADIUS2 = SAFETY_FRIEND_RADIUS * SAFETY_FRIEND_RADIUS;

    constexpr int   MIN_ENEMIES_FOR_GRENADE = 2;

    constexpr float UNDER_FIRE_GRENADE_FACTOR = 1.00f; 
}

static int countEnemiesAround(int cr, int cc, float radius2)
{
    int count = 0;
    for (const auto* other : g_units) {
        if (!other->isAlive) continue;
        if (other->team == Definitions::Team::Blue || other->team == Definitions::Team::Orange) {
            int dr = other->row - cr;
            int dc = other->col - cc;
            float d2 = float(dr * dr + dc * dc);
        }
    }
    return 0; 
}

static int countEnemiesAroundVsTeam(Definitions::Team selfTeam, int cr, int cc, float radius2)
{
    int count = 0;
    for (const auto* other : g_units) {
        if (!other->isAlive) continue;
        if (other->team == selfTeam) continue; 
        int dr = other->row - cr;
        int dc = other->col - cc;
        float d2 = float(dr * dr + dc * dc);
        if (d2 <= radius2) ++count;
    }
    return count;
}

static bool hasFriendlyNear(Definitions::Team selfTeam, int cr, int cc, float radius2)
{
    for (const auto* other : g_units) {
        if (!other->isAlive) continue;
        if (other->team != selfTeam) continue; 
        int dr = other->row - cr;
        int dc = other->col - cc;
        float d2 = float(dr * dr + dc * dc);
        if (d2 <= radius2) return true;
    }
    return false;
}


namespace Models {

    static bool isEnemyInRange(const Models::Unit* self, int range) {
        const int rangeSq = range * range;
        for (const auto* other : g_units) {
            if (!other->isAlive || other->team == self->team) continue;

            int dr = other->row - self->row;
            int dc = other->col - self->col;
            if ((dr * dr + dc * dc) <= rangeSq) {
                return true; 
            }
        }
        return false;
    }

    Warrior::Warrior(Definitions::Team t, int r0, int c0)
        : Unit(t, Definitions::Role::Warrior, r0, c0) {
    }

    char Warrior::roleLetter() const { return 'W'; }

    bool Warrior::acquireVisibleEnemy(int& outR, int& outC) const {
        outR = -1; outC = -1;
        if (!isAlive) return false;

        const int R2 = Definitions::SIGHT_RANGE * Definitions::SIGHT_RANGE;
        float bestD2 = 1e12f;

        for (const auto* other : g_units) {
            if (!other->isAlive || other->team == this->team) continue;
            int dr = other->row - this->row;
            int dc = other->col - this->col;
            float d2 = float(dr * dr + dc * dc);
            if (d2 > R2 || d2 >= bestD2) continue;
            if (!AI::Visibility::HasLineOfSight(g_grid, this->row, this->col, other->row, other->col)) continue;

            bestD2 = d2;
            outR = other->row;
            outC = other->col;
        }
        return (outR >= 0);
    }

    void Warrior::CheckAndReportStatus() {
        if (!isAlive) return;

        if (m_grenadeCooldownTicks > 0)
            --m_grenadeCooldownTicks;
        
         ConsiderAutoGrenade();

        // Autonomous logic (when commander is down)
        if (isAutonomous) {
            AI::State* current_state = m_fsm ? m_fsm->GetCurrentState() : nullptr;
            if (current_state && !current_state->CanReport()) {
                return;
            }

            int vis_er = -1, vis_ec = -1;
            bool visible_target_found = acquireVisibleEnemy(vis_er, vis_ec);

            int near_er = -1, near_ec = -1;
            bool nearest_target_found = false;
            if (!visible_target_found) {
                float nearest_d2 = std::numeric_limits<float>::infinity();
                for (const auto* other : g_units) {
                    if (!other->isAlive || other->team == this->team) continue;
                    int dr = other->row - this->row;
                    int dc = other->col - this->col;
                    float d2 = float(dr * dr + dc * dc);
                    if (d2 < nearest_d2) {
                        nearest_d2 = d2;
                        near_er = other->row;
                        near_ec = other->col;
                    }
                }
                nearest_target_found = (near_er != -1);
            }

            int target_r = -1, target_c = -1;
            bool target_chosen = false;
            if (visible_target_found) {
                target_r = vis_er; target_c = vis_ec; target_chosen = true;
            }
            else if (nearest_target_found) {
                target_r = near_er; target_c = near_ec; target_chosen = true;
            }

            bool target_reachable = false;
            if (target_chosen) {
                AI::Pathfinding::Path path_check = AI::Pathfinding::AStar_FindPath(
                    this, g_grid, g_smap,
                    { this->row, this->col }, { target_r, target_c },
                    Definitions::ASTAR_RISK_WEIGHT
                );
                target_reachable = !path_check.empty();
            }

            bool is_attacking = dynamic_cast<AI::State_Attacking*>(current_state) != nullptr;
            bool is_moving_to_attack = dynamic_cast<AI::State_MovingToTarget*>(current_state) != nullptr;

            if (target_chosen && target_reachable) {
                if (!is_attacking && !is_moving_to_attack) {
                    m_fsm->ChangeState(new AI::State_Attacking(target_r, target_c, &g_combat));
                }
                return;
            }
            else if (target_chosen && !target_reachable) {
                bool is_moving = dynamic_cast<AI::State_MovingToTarget*>(current_state) != nullptr;
                if (!is_moving) {
                    m_fsm->ChangeState(new AI::State_MovingToTarget(target_r, target_c, new AI::State_Idle()));
                }
                return;
            }
            else {
                const float maxRisk = g_smap.maxValue();
                const float hereRisk = (maxRisk > 0.001f) ? (g_smap.at(row, col) / maxRisk) : 0.0f;
                if (hereRisk >= Definitions::WARRIOR_UNDER_FIRE_THRESHOLD * 0.8f) {
                    if (dynamic_cast<AI::State_Defending*>(current_state) == nullptr) {
                        m_fsm->ChangeState(new AI::State_Defending(row, col));
                    }
                    return;
                }

                if (dynamic_cast<AI::State_Idle*>(current_state) == nullptr) {
                    m_fsm->ChangeState(new AI::State_Idle());
                }
                return;
            }
        }

        // Standard reporting logic (when under commander's control)
        if (m_fsm && !m_fsm->GetCurrentState()->CanReport()) {
            return;
        }

        if (stats.ammo <= 0) {
            if (isEnemyInRange(this, Definitions::FIRE_RANGE)) {
                if (!m_reportedLowAmmo) {
                    report(AI::EventType::LowAmmo, row, col, stats.ammo);
                    m_reportedLowAmmo = true;
                }
                printf("Unit %d: Out of ammo AND under threat. Retreating to cover!\n", id);
                m_fsm->ChangeState(new AI::State_RetreatingToCover());
                return;
            }
        }

        const int reportThreshold = (Definitions::HP_MAX * 3) / 4; // 75% of max HP
        if (stats.hp <= reportThreshold && !m_reportedInjured) {
            report(AI::EventType::Injured, row, col, stats.hp);
            m_reportedInjured = true;
        }
        else if (stats.hp > reportThreshold) {
            m_reportedInjured = false;
        }

        const float riskThreshold = Definitions::WARRIOR_UNDER_FIRE_THRESHOLD;
        const float maxRisk = g_smap.maxValue();
        const float normalizedRisk = (maxRisk > 0.001f) ? (g_smap.at(row, col) / maxRisk) : 0.0f;

        if (normalizedRisk >= riskThreshold && !m_reportedUnderFire) {
            m_reportedUnderFire = true;
            report(AI::EventType::UnderFire, row, col);
        }
        else if (normalizedRisk < riskThreshold && m_reportedUnderFire) {
            m_reportedUnderFire = false;
        }

        if (stats.ammo <= 0 && !m_reportedLowAmmo) {
            report(AI::EventType::LowAmmo, row, col, stats.ammo);
            m_reportedLowAmmo = true;
        }
        else if (stats.ammo > 0) {
            m_reportedLowAmmo = false;
        }

        if (m_sightCooldown > 0) --m_sightCooldown;

        int er = -1, ec = -1;
        if (acquireVisibleEnemy(er, ec) && m_sightCooldown == 0) {
            report(AI::EventType::EnemySighted, er, ec, 0);
            m_sightCooldown = 20; // Cooldown to prevent spam
        }
    }

    bool Models::Warrior::TryThrowGrenade(int targetRow, int targetCol)
    {
        if (!isAlive) return false;

        int& grenades = stats.grenades;
        if (grenades <= 0) return false;

        const float r0 = static_cast<float>(row);
        const float c0 = static_cast<float>(col);
        const float rT = static_cast<float>(targetRow);
        const float cT = static_cast<float>(targetCol);

        const float dr = rT - r0;
        const float dc = cT - c0;
        const float d2 = dr * dr + dc * dc;

        if (d2 < GRENADE_MIN_THROW_DIST2) {
            g_combat.dropGrenade(r0, c0, g_grid, team);
        }
        else {
            g_combat.throwGrenadeParabola(r0, c0, rT, cT, team);
        }

        --grenades;
        return true;
    }

    void Models::Warrior::ConsiderAutoGrenade()
    {
        if (!isAlive) return;
        if (stats.grenades <= 0) return;
        if (m_grenadeCooldownTicks > 0) return;

        int er = -1, ec = -1;
        if (!acquireVisibleEnemy(er, ec)) return;

        if (!isEnemyInRange(this, Definitions::FIRE_RANGE)) return;

        const float r0 = static_cast<float>(row);
        const float c0 = static_cast<float>(col);
        const float dr = static_cast<float>(er) - r0;
        const float dc = static_cast<float>(ec) - c0;
        const float d2 = dr * dr + dc * dc;
        if (d2 > GRENADE_MAX_THROW_DIST2) return;

        const float maxRisk = g_smap.maxValue();
        const float hereRisk = (maxRisk > 1e-5f) ? (g_smap.at(row, col) / maxRisk) : 0.0f;
        const float fireThreshold = Definitions::WARRIOR_UNDER_FIRE_THRESHOLD * UNDER_FIRE_GRENADE_FACTOR;
        const bool  underFire = (hereRisk >= fireThreshold);

        const int enemiesNearTarget =
            countEnemiesAroundVsTeam(this->team, er, ec, DECISION_AOE_RADIUS2);

        const bool friendTooClose =
            hasFriendlyNear(this->team, er, ec, SAFETY_FRIEND_RADIUS2);

        if (friendTooClose) return;

        if (enemiesNearTarget >= MIN_ENEMIES_FOR_GRENADE || underFire) {
            if (TryThrowGrenade(er, ec)) {
                m_grenadeCooldownTicks = GRENADE_AI_COOLDOWN_TICKS;
            }
        }
    }


} // namespace Models
