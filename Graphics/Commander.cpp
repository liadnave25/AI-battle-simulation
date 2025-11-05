#include "Commander.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <cmath>
#include <limits>
#include <cstdint>

#include "Combat.h"
#include "StateMachine.h"
#include "State.h"
#include "State_Attacking.h"
#include "State_Defending.h"
#include "State_MovingToTarget.h"
#include "State_Idle.h"
#include "State_Healing.h"
#include "State_Supplying.h"
#include "State_RefillAtDepot.h"
#include "Globals.h"
#include "Visibility.h"
#include "Pathfinding.h"
#include "Definitions.h"

using namespace AI;
using Definitions::Team;
using Definitions::Role;

static constexpr int ENEMY_TTL_FRAMES = 360;
static constexpr int MERGE_DIST2 = 2 * 2;
static int s_frameCounter = 0;
static constexpr int SUPPORT_LOCK_FRAMES = 480; 

static bool s_announcedCommanderDownBlue = false;
static bool s_announcedCommanderDownOrange = false;

static inline Models::Unit* findUnitById(int uid) {
    if (uid <= 0) return nullptr;
    auto it = std::find_if(g_units.begin(), g_units.end(),
        [uid](const Models::Unit* u) { return u->id == uid; });
    return (it != g_units.end()) ? (*it) : nullptr;
}

static inline bool isOurHalf(int , int c, int gridSize, Team team) {
    return (team == Team::Blue) ? (c < gridSize / 2) : (c >= gridSize / 2);
}

static inline const char* teamTag(Team t) {
    return (t == Team::Blue ? "Blue" : "Orange");
}

static inline bool isPoisonPtr(const void* p) {
#if defined(_MSC_VER)
    const uintptr_t v = reinterpret_cast<uintptr_t>(p);
    return v == 0xDDDDDDDDu || v == 0xFEEEFEEEu;
#else
    (void)p; return false;
#endif
}

static inline bool isPointerIntoGUnits(const Models::Unit* p) {
    if (!p) return false;
    if (g_units.empty()) return false;
    auto it = std::find(g_units.begin(), g_units.end(), p);
    return (it != g_units.end());
}

static inline char roleChar(const Models::Unit* u) {
    if (!u) return '?';
    switch (u->role) {
    case Role::Commander: return 'C';
    case Role::Warrior:   return 'W';
    case Role::Medic:     return 'M';
    case Role::Supplier:  return 'P';
    default:              return '?';
    }
}

static inline bool isSaneUnit(const Models::Unit* u) {
    if (!u) return false;
    if (!u->isAlive) return false;
    if (u->row < 0 || u->row >= Definitions::GRID_SIZE) return false;
    if (u->col < 0 || u->col >= Definitions::GRID_SIZE) return false;
    if (u->role < Role::Commander || u->role > Role::Supplier) return false;
    return true;
}

static inline bool isInterruptible(Models::Unit* u) {
    if (!u || !u->isAlive) return false;
    auto* fsm = u->m_fsm;
    if (!fsm || isPoisonPtr(fsm)) return false;
    AI::State* s = fsm->GetCurrentState();
    if (!s || isPoisonPtr(s)) return false;
    return s->CanReport();
}

static inline bool lockedToOtherTarget(const Models::Unit* u, int wantedTargetId, bool isMedic, int nowFrame) {
    if (!u) return false;
    const int current = isMedic ? u->assignedHealTargetId : u->assignedSupplyTargetId;
    if (current == -1) return false;             
    if (current == wantedTargetId) return false; 
    return (u->supportLockUntilFrame > nowFrame); 
}

static inline void maybeExpireLock(Models::Unit* u, int nowFrame) {
    if (!u) return;
    if (u->supportLockUntilFrame <= nowFrame) {
        u->assignedHealTargetId = -1;
        u->assignedSupplyTargetId = -1;
    }
}

void Commander::initSubscriptions() {
    EventBus::instance().subscribe(-1, [this](const Message& m) { onMessage(m); });
    if (unitId > 0) {
        EventBus::instance().subscribe(unitId, [this](const Message& m) { onMessage(m); });
    }
}

void Commander::onMessage(const Message& m) {
    if (m.fromUnitId > 0) {
        if (auto* src = findUnitById(m.fromUnitId)) {
            if (src->team != myTeam) return;
        }
        else {
            return;
        }
    }

    if (m.type == EventType::EnemySighted) {
        bool merged = false;
        for (auto& s : knownEnemies) {
            int dr = s.row - m.row, dc = s.col - m.col;
            int d2 = dr * dr + dc * dc;
            if (d2 <= MERGE_DIST2) {
                s.row = m.row; s.col = m.col;
                s.lastSeenFrame = s_frameCounter;
                merged = true;
                break;
            }
        }
        if (!merged) {
            knownEnemies.push_back({ m.row, m.col, s_frameCounter });
            std::printf("[CMD/%s] New EnemySighted at (%d,%d)\n", teamTag(myTeam), m.row, m.col);
        }
        return;
    }

    if (m.type == EventType::UnderFire) {
        if (std::find(underFireUnits.begin(), underFireUnits.end(), m.fromUnitId) == underFireUnits.end()) {
            underFireUnits.push_back(m.fromUnitId);
            std::printf("[CMD/%s] Unit %d reports UNDER FIRE.\n", teamTag(myTeam), m.fromUnitId);
        }
        return;
    }

    if (m.type == EventType::LowAmmo) {
        if (std::find(lowAmmoUnits.begin(), lowAmmoUnits.end(), m.fromUnitId) == lowAmmoUnits.end()) {
            lowAmmoUnits.push_back(m.fromUnitId);
            std::printf("[CMD/%s] Unit %d reports LOW AMMO.\n", teamTag(myTeam), m.fromUnitId);
        }
        return;
    }

    if (m.type == EventType::Injured) {
        if (std::find(injuredUnits.begin(), injuredUnits.end(), m.fromUnitId) == injuredUnits.end()) {
            injuredUnits.push_back(m.fromUnitId);
            std::printf("[CMD/%s] Unit %d reports INJURED (HP: %d).\n", teamTag(myTeam), m.fromUnitId, m.extra);
        }
        return;
    }
}

bool Commander::shouldIssueNow(const Models::Unit* u, const Order& o, int frameCounter) const {
    if (!u) return false;
    auto it = m_lastOrders.find(u->id);
    if (it == m_lastOrders.end()) return true;

    const auto& cached = it->second;
    if (sameOrder(cached.order, o) &&
        cached.frameIssued >= 0 &&
        (frameCounter - cached.frameIssued) < ORDER_COOLDOWN_FRAMES)
    {
        return false;
    }
    return true;
}

void Commander::rememberIssued(const Models::Unit* u, const Order& o, int frameCounter) {
    if (!u) return;
    m_lastOrders[u->id] = CachedOrder{ o, frameCounter };
}

void Commander::issueOrder(Models::Unit* u, const Order& o, int frameCounter) {
    if (!u || !u->isAlive) return;

    if (!isPointerIntoGUnits(u) || !isSaneUnit(u)) {
        std::printf("[CMD/%s] SKIP order to UnitPtr=%p: invalid/suspect unit.\n",
            teamTag(myTeam), reinterpret_cast<void*>(u));
        return;
    }

    auto* fsm = u->m_fsm;
    if (!fsm || isPoisonPtr(fsm)) {
        std::printf("[CMD/%s] SKIP order to Unit#%d: FSM invalid.\n", teamTag(myTeam), u->id);
        return;
    }

    if (u->team != myTeam) {
        std::printf("[CMD/%s] ERROR: Tried to issue order to enemy Unit#%d!\n", teamTag(myTeam), u->id);
        return;
    }

    AI::State* currentState = fsm->GetCurrentState();
    if (currentState && !isPoisonPtr(currentState) && !currentState->CanReport()) {
        if (o.type == OrderType::AttackTo || o.type == OrderType::DefendAt || o.type == OrderType::MoveTo) {
            std::printf("[CMD/%s] SKIP order %d for Unit#%d, unit is busy (cannot report).\n",
                teamTag(myTeam), (int)o.type, u->id);
            return;
        }
    }

    if (!shouldIssueNow(u, o, frameCounter)) return;

    const int uid = u->id;
    const char roleCh = roleChar(u);
    const int tr = o.row, tc = o.col, tu = o.targetUnitId, ot = (int)o.type;

    std::printf("[CMD/%s] -> Unit#%d (%c) Order=%d Target=(%d,%d) TargetUnit=%d\n",
        teamTag(myTeam), uid, roleCh, ot, tr, tc, tu);

    EventBus::instance().publish(Message{
        EventType::OrderIssued, -1, uid, tr, tc, ot
        });

    extern Combat::System g_combat;
    switch (o.type) {
    case OrderType::AttackTo:
        fsm->ChangeState(new State_Attacking(tr, tc, &g_combat));
        break;
    case OrderType::DefendAt:
        fsm->ChangeState(new State_Defending(tr, tc));
        break;
    case OrderType::MoveTo:
        fsm->ChangeState(new State_MovingToTarget(tr, tc, nullptr));
        break;
    default:
        fsm->ChangeState(new State_Idle());
        break;
    }

    rememberIssued(u, o, frameCounter);
}

bool Commander::pickLiveVisibleTarget(const std::vector<Models::Unit*>& enemies,
    int& outR, int& outC) const
{
    outR = -1; outC = -1;
    Models::Unit* self = findUnitById(this->unitId);
    if (!self || !self->isAlive) return false;

    float bestScore = std::numeric_limits<float>::infinity();

    for (const auto* enemy : enemies) {
        if (!enemy || !enemy->isAlive) continue;
        if (AI::Visibility::HasLineOfSight(g_grid, self->row, self->col, enemy->row, enemy->col)) {
            int dr = enemy->row - self->row;
            int dc = enemy->col - self->col;
            float distSq = float(dr * dr + dc * dc);

            float score = distSq;

            int enemyCell = g_grid.at(enemy->row, enemy->col);
            if (enemyCell == Definitions::Cell::TREE) {
                score *= Definitions::TREE_COVER_PENALTY_MULTIPLIER;
            }

            if (score < bestScore) {
                bestScore = score;
                outR = enemy->row;
                outC = enemy->col;
            }
        }
    }
    return (outR != -1 && outC != -1);
}

void Commander::decideAndIssueOrders(std::vector<Models::Unit*>& myTeamPtrs,
    std::vector<Models::Unit*>& enemies,
    int frameCounter)
{
    int targetR = -1, targetC = -1;
    if (!pickLiveVisibleTarget(enemies, targetR, targetC)) {
        if (!knownEnemies.empty()) {
            int mostRecent = -1;
            for (const auto& e : knownEnemies) {
                if (e.lastSeenFrame > mostRecent) {
                    mostRecent = e.lastSeenFrame;
                    targetR = e.row; targetC = e.col;
                }
            }
        }
    }

    std::unordered_set<int> reservedIds;

    auto removeInvalid = [&](std::vector<int>& list) {
        list.erase(std::remove_if(list.begin(), list.end(), [](int id) {
            Models::Unit* u = findUnitById(id);
            return !u || !u->isAlive;
            }), list.end());
        };
    removeInvalid(underFireUnits);
    removeInvalid(injuredUnits);
    removeInvalid(lowAmmoUnits);

    std::vector<int> underFireCopy = underFireUnits;
    for (int uid : underFireCopy) {
        auto it = std::find(underFireUnits.begin(), underFireUnits.end(), uid);
        if (it == underFireUnits.end()) continue;

        Models::Unit* u = findUnitById(uid);
        if (u && isInterruptible(u) && reservedIds.find(u->id) == reservedIds.end()) {
            issueOrder(u, Order{ OrderType::DefendAt, u->row, u->col }, frameCounter);
            reservedIds.insert(u->id);
            underFireUnits.erase(it);
        }
    }

    std::vector<Models::Unit*> availableMedics;
    std::vector<Models::Unit*> availableSuppliers;
    for (auto* u : myTeamPtrs) {
        if (!u || !u->isAlive || u->team != myTeam || reservedIds.count(u->id) || !isInterruptible(u)) continue;
        if (u->role == Role::Medic)    availableMedics.push_back(u);
        if (u->role == Role::Supplier) availableSuppliers.push_back(u);
    }

    std::vector<int> injuredCopy = injuredUnits;
    for (int injId : injuredCopy) {
        if (availableMedics.empty()) break;
        auto injIt = std::find(injuredUnits.begin(), injuredUnits.end(), injId);
        if (injIt == injuredUnits.end()) continue;

        Models::Unit* inj = findUnitById(injId);
        if (inj && inj->isAlive) {

            Models::Unit* medic = nullptr;
            for (size_t i = 0; i < availableMedics.size(); ++i) {
                Models::Unit* cand = availableMedics[i];
                maybeExpireLock(cand, frameCounter);
                if (!lockedToOtherTarget(cand, injId,true, frameCounter)) {
                    medic = cand;
                    availableMedics.erase(availableMedics.begin() + i);
                    break;
                }
            }
            if (!medic) continue; 

            if (medic->roleData.currentHealPool < Definitions::MEDIC_REFILL_THRESHOLD) {
                medic->m_fsm->ChangeState(new State_RefillAtDepot(Role::Medic, injId));
                std::printf("[CMD/%s] -> Unit#%d (M) REFILL then heal Unit#%d (handled inside state)\n",
                    teamTag(myTeam), medic->id, injId);
            }
            else {
                medic->m_fsm->ChangeState(new State_Healing(injId));
                std::printf("[CMD/%s] -> Unit#%d (M) HEAL Unit#%d\n",
                    teamTag(myTeam), medic->id, injId);
            }

            medic->assignedHealTargetId = injId;
            medic->supportLockUntilFrame = frameCounter + SUPPORT_LOCK_FRAMES;

            rememberIssued(medic, Order{ OrderType::Heal, -1, -1, injId }, frameCounter);
            reservedIds.insert(medic->id);
            injuredUnits.erase(injIt);
        }
        else {
            injuredUnits.erase(injIt);
        }
    }

    std::vector<int> lowAmmoCopy = lowAmmoUnits;
    for (int needyId : lowAmmoCopy) {
        if (availableSuppliers.empty()) break;
        auto needyIt = std::find(lowAmmoUnits.begin(), lowAmmoUnits.end(), needyId);
        if (needyIt == lowAmmoUnits.end()) continue;

        Models::Unit* needy = findUnitById(needyId);
        if (needy && needy->isAlive) {

            Models::Unit* supplier = nullptr;
            for (size_t i = 0; i < availableSuppliers.size(); ++i) {
                Models::Unit* cand = availableSuppliers[i];
                maybeExpireLock(cand, frameCounter);
                if (!lockedToOtherTarget(cand, needyId, false, frameCounter)) {
                    supplier = cand;
                    availableSuppliers.erase(availableSuppliers.begin() + i);
                    break;
                }
            }
            if (!supplier) continue; 

            if (supplier->roleData.currentAmmo < Definitions::SUPPLIER_REFILL_THRESHOLD) {
                supplier->m_fsm->ChangeState(new State_RefillAtDepot(Role::Supplier, needyId));
                std::printf("[CMD/%s] -> Unit#%d (P) REFILL then resupply Unit#%d (handled inside state)\n",
                    teamTag(myTeam), supplier->id, needyId);
            }
            else {
                supplier->m_fsm->ChangeState(new State_Supplying(needyId));
                std::printf("[CMD/%s] -> Unit#%d (P) SUPPLY Unit#%d\n",
                    teamTag(myTeam), supplier->id, needyId);
            }

            supplier->assignedSupplyTargetId = needyId;
            supplier->supportLockUntilFrame = frameCounter + SUPPORT_LOCK_FRAMES;

            rememberIssued(supplier, Order{ OrderType::Resupply, -1, -1, needyId }, frameCounter);
            reservedIds.insert(supplier->id);
            lowAmmoUnits.erase(needyIt);
        }
        else {
            lowAmmoUnits.erase(needyIt);
        }
    }

    for (auto* u : myTeamPtrs) {
        if (!u || !u->isAlive || u->team != myTeam) continue;
        if (reservedIds.count(u->id) || !isInterruptible(u)) continue;

        if (u->role == Role::Warrior) {
            if (u->stats.ammo <= 0) continue;

            Order o{};
            if (targetR != -1 && targetC != -1) {
                o.type = OrderType::AttackTo; o.row = targetR; o.col = targetC;
            }
            else if (hasAnchor()) {
                o.type = OrderType::DefendAt; o.row = anchorR; o.col = anchorC;
            }
            else {
                o.type = OrderType::MoveTo; o.row = Definitions::GRID_SIZE / 2; o.col = Definitions::GRID_SIZE / 2;
            }
            issueOrder(u, o, frameCounter);
        }
        else if (u->role == Role::Commander) {
            if (hasAnchor() && (u->row != anchorR || u->col != anchorC)) {
                issueOrder(u, Order{ OrderType::MoveTo, anchorR, anchorC }, frameCounter);
                reservedIds.insert(u->id);
            }
        }
    }
}

void Commander::tick(Models::Grid& map,
    std::vector<Models::Unit*>& myTeamPtrs,
    std::vector<Models::Unit*>& enemyPtrs,
    int frameCounter)
{
    s_frameCounter = frameCounter;

    Models::Unit* self = findUnitById(this->unitId);

    if (!self || !self->isAlive) {
        bool& announced = (myTeam == Team::Blue) ? s_announcedCommanderDownBlue : s_announcedCommanderDownOrange;
        if (!announced) {
            EventBus::instance().publish(Message{ EventType::CommanderDown, /*from*/ unitId, /*to*/ -1 });
            std::printf("[CMD/%s] Commander is DOWN — switching units to autonomy.\n", teamTag(myTeam));
            announced = true;
        }
        return;
    }

    if (self && self->isFighting) {
        FightAsWarrior();
        return;
    }

    knownEnemies.erase(
        std::remove_if(knownEnemies.begin(), knownEnemies.end(),
            [frameCounter](const EnemyInfo& e) { return (frameCounter - e.lastSeenFrame) > ENEMY_TTL_FRAMES; }),
        knownEnemies.end()
    );

    bool enemiesExistOnMap = false;
    for (const auto* en : enemyPtrs) { if (en && en->isAlive) { enemiesExistOnMap = true; break; } }
    if (!enemiesExistOnMap) {
        if (!knownEnemies.empty()) knownEnemies.clear();
    }

    auto calculateHybridRiskForThisCommander =
        [&](int r, int c) -> float
        {
            const float PROXIMITY_THREAT_RADIUS_SQ = 15.0f * 15.0f;
            const float PROXIMITY_WEIGHT = 1.0f;
            const float SMAP_NORMALIZATION = std::max(0.001f, g_smap.maxValue());

            float smapRisk = g_smap.at(r, c) / SMAP_NORMALIZATION;
            float proxRisk = 0.0f;
            float minEnemyDistSq = std::numeric_limits<float>::infinity();

            for (const auto* en : enemyPtrs) {
                if (!en || !en->isAlive) continue;
                float dr = (float)en->row - r;
                float dc = (float)en->col - c;
                minEnemyDistSq = std::min(minEnemyDistSq, dr * dr + dc * dc);
            }

            if (minEnemyDistSq <= PROXIMITY_THREAT_RADIUS_SQ) {
                float dist = std::sqrt(minEnemyDistSq);
                proxRisk = (1.0f - (dist / std::sqrt(PROXIMITY_THREAT_RADIUS_SQ))) * PROXIMITY_WEIGHT;
            }
            return std::max(smapRisk, proxRisk);
        };

    if (safetyMonitor(calculateHybridRiskForThisCommander, frameCounter, map)) {
        if (self && !self->isFighting && isInterruptible(self)) {
            issueOrder(self, Order{ OrderType::MoveTo, anchorR, anchorC }, frameCounter);
        }
    }

    if (frameCounter % 30 == 0) {
        decideAndIssueOrders(myTeamPtrs, enemyPtrs, frameCounter);
    }
}

void Commander::FightAsWarrior() {
    Models::Unit* self = findUnitById(this->unitId);
    if (!self || !self->isAlive) return;

    extern Combat::System g_combat;
    AI::StateMachine* fsm = self->m_fsm;
    if (!fsm || isPoisonPtr(fsm)) return;

    int vis_er = -1, vis_ec = -1;
    float nearest_vis_d2 = std::numeric_limits<float>::infinity();
    const int SIGHT_RANGE_SQ = Definitions::SIGHT_RANGE * Definitions::SIGHT_RANGE;

    for (const auto* other : g_units) {
        if (!other->isAlive || other->team == self->team) continue;
        int dr = other->row - self->row;
        int dc = other->col - self->col;
        float d2 = float(dr * dr + dc * dc);
        if (d2 > SIGHT_RANGE_SQ || d2 >= nearest_vis_d2) continue;

        if (AI::Visibility::HasLineOfSight(g_grid, self->row, self->col, other->row, other->col)) {

            float score = d2;
            int enemyCell = g_grid.at(other->row, other->col);
            if (enemyCell == Definitions::Cell::TREE) {
                score *= Definitions::TREE_COVER_PENALTY_MULTIPLIER;
            }

            if (score < nearest_vis_d2) {
                nearest_vis_d2 = score;
                vis_er = other->row;
                vis_ec = other->col;
            }
        }
    }

    if (vis_er != -1) {
        AI::State* currentState = fsm->GetCurrentState();
        if (dynamic_cast<State_Attacking*>(currentState) == nullptr) {
            fsm->ChangeState(new State_Attacking(vis_er, vis_ec, &g_combat));
        }
        return;
    }

    int nearest_er = -1, nearest_ec = -1;
    float nearest_d2 = std::numeric_limits<float>::infinity();

    for (const auto* other : g_units) {
        if (!other->isAlive || other->team == self->team) continue;
        int dr = other->row - self->row;
        int dc = other->col - self->col;

        float d2 = float(dr * dr + dc * dc);
        float score = d2;
        int enemyCell = g_grid.at(other->row, other->col);
        if (enemyCell == Definitions::Cell::TREE) {
            score *= Definitions::TREE_COVER_PENALTY_MULTIPLIER;
        }

        if (score < nearest_d2) {
            nearest_d2 = score;
            nearest_er = other->row;
            nearest_ec = other->col;
        }
    }

    AI::State* currentState = fsm->GetCurrentState();
    bool isIdle = (dynamic_cast<State_Idle*>(currentState) != nullptr);
    bool isMoving = (dynamic_cast<State_MovingToTarget*>(currentState) != nullptr);

    if (nearest_er != -1) {
        AI::Pathfinding::Path path_check = AI::Pathfinding::AStar_FindPath(
            self, g_grid, g_smap,
            { self->row, self->col },
            { nearest_er, nearest_ec },
            AI::Pathfinding::RiskWeightForUnit(self)
        );
        bool target_reachable = !path_check.empty();

        if (target_reachable) {
            if (isIdle) {
                fsm->ChangeState(new State_Attacking(nearest_er, nearest_ec, &g_combat));
            }
        }
        else {
            if (isIdle) {
                int centerR = Definitions::GRID_SIZE / 2;
                int centerC = Definitions::GRID_SIZE / 2;
                fsm->ChangeState(new State_MovingToTarget(centerR, centerC, nullptr));
            }
        }
    }
    else {
        if (!isIdle && !isMoving) {
            fsm->ChangeState(new State_Idle());
        }
    }
}

bool Commander::selectDefensiveAnchor(const Models::Grid& map,
    const std::function<float(int, int)>& riskAt)
{
    const int gridSize = Definitions::GRID_SIZE;
    int bestR = -1, bestC = -1; float bestRisk = std::numeric_limits<float>::infinity();

    for (int k = 0; k < ANCHOR_RETRIES; ++k) {
        int r = std::rand() % gridSize;
        int c = std::rand() % gridSize;
        if (!AI::Pathfinding::inPlayfield(r, c)) continue;
        if (!isOurHalf(r, c, gridSize, myTeam)) continue;
        if (!AI::Pathfinding::IsWalkableForMovement(map.at(r, c))) continue;

        float risk = riskAt(r, c);
        if (risk <= SAFE_RISK_MAX && risk < bestRisk) {
            bestRisk = risk; bestR = r; bestC = c;
            if (bestRisk <= 0.05f) break;
        }
    }

    if (bestR < 0) {
        bestRisk = std::numeric_limits<float>::infinity();
        for (int k = 0; k < ANCHOR_RETRIES; ++k) {
            int r = std::rand() % gridSize;
            int c = std::rand() % gridSize;
            if (!AI::Pathfinding::inPlayfield(r, c)) continue;
            if (!isOurHalf(r, c, gridSize, myTeam)) continue;
            if (!AI::Pathfinding::IsWalkableForMovement(map.at(r, c))) continue;

            float risk = riskAt(r, c);
            if (risk < bestRisk) {
                bestRisk = risk; bestR = r; bestC = c;
            }
        }
    }

    if (bestR >= 0) {
        anchorR = bestR; anchorC = bestC;
        std::printf("[CMD/%s] Selected new anchor at (%d,%d), Risk=%.3f\n",
            teamTag(myTeam), anchorR, anchorC, bestRisk);
        return true;
    }

    anchorR = anchorC = -1;
    return false;
}

bool Commander::safetyMonitor(const std::function<float(int, int)>& riskAt,
    int frameCounter,
    const Models::Grid& map)
{
    if (frameCounter % SAFE_RECHECK_INTERVAL != 0) return false;

    if (!hasAnchor()) {
        if (selectDefensiveAnchor(map, riskAt)) {
            lastReanchorFrame = frameCounter;
            return true;
        }
        return false;
    }

    const float currentRisk = riskAt(anchorR, anchorC);
    const float reanchorThreshold = SAFE_RISK_MAX + SAFE_HYSTERESIS;

    if (currentRisk > reanchorThreshold && (frameCounter - lastReanchorFrame) >= REANCHOR_COOLDOWN_FRAMES) {
        if (selectDefensiveAnchor(map, riskAt)) {
            lastReanchorFrame = frameCounter;
            return true;
        }
        else {
            lastReanchorFrame = frameCounter - (REANCHOR_COOLDOWN_FRAMES / 2);
        }
    }
    return false;
}

void AI::Commander::setUnitId(int id) {
    this->unitId = id;
}
