#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include "Orders.h"
#include "AIEvents.h"
#include "EventBus.h"
#include "Units.h"
#include "Grid.h"
#include "Definitions.h"
#include "Visibility.h"
#include <limits>

namespace AI {

    struct EnemyInfo {
        int row = -1, col = -1;
        int lastSeenFrame = 0;
    };

    class Commander {
    public:
        explicit Commander(Definitions::Team team = Definitions::Team::Blue)
            : myTeam(team) {
        }

        void initSubscriptions();

        void tick(Models::Grid& map,
            std::vector<Models::Unit*>& myTeamPtrs,
            std::vector<Models::Unit*>& enemyPtrs,
            int frameCounter);

        void setUnitId(int id);

        static constexpr float SAFE_RISK_MAX = Definitions::COMMANDER_SAFE_RISK_MAX;   
        static constexpr int   ANCHOR_RETRIES = Definitions::COMMANDER_ANCHOR_RETRIES;
        int anchorR = -1; 
        int anchorC = -1; 

        bool selectDefensiveAnchor(const Models::Grid& map,
            const std::function<float(int, int)>& riskAt);

        inline bool hasAnchor() const { return anchorR >= 0 && anchorC >= 0; }
        inline void getAnchor(int& r, int& c) const { r = anchorR; c = anchorC; }

        static constexpr int   SAFE_RECHECK_INTERVAL = 30;     
        static constexpr float SAFE_HYSTERESIS = 0.05f;          
        static constexpr int   REANCHOR_COOLDOWN_FRAMES = 180;   
        int lastReanchorFrame = -REANCHOR_COOLDOWN_FRAMES;        

        bool safetyMonitor(const std::function<float(int, int)>& riskAt,
            int frameCounter,
            const Models::Grid& map);

        inline Definitions::Team team() const { return myTeam; }

        inline const AI::Visibility::BArray& teamVisibility() const { return m_teamVis; }

    private:
        Definitions::Team myTeam; 
        int unitId = -1;        

        std::vector<EnemyInfo> knownEnemies; 
        std::vector<int> lowAmmoUnits;   
        std::vector<int> injuredUnits;   
        std::vector<int> underFireUnits; 

        AI::Visibility::BArray m_teamVis{}; 
        void rebuildTeamVisibility(const Models::Grid& map,
            const std::vector<Models::Unit*>& units);

        void onMessage(const AI::Message& m);

        void decideAndIssueOrders(std::vector<Models::Unit*>& myTeam,
            std::vector<Models::Unit*>& enemies,
            int frameCounter);

        bool pickLiveVisibleTarget(const std::vector<Models::Unit*>& enemies,
            int& outR, int& outC) const;

        struct CachedOrder {
            Order order{};          
            int   frameIssued = -1; 
        };
        std::unordered_map<int, CachedOrder> m_lastOrders; 

        static constexpr int ORDER_COOLDOWN_FRAMES = 45;

        static constexpr int HYST_ATTACK_CELLS = 3;
        static constexpr int HYST_DEFEND_CELLS = 2;
        static constexpr int HYST_MOVE_CELLS = 2;

        static inline int manhattan(int r1, int c1, int r2, int c2) {
            return (r1 >= 0 && c1 >= 0 && r2 >= 0 && c2 >= 0)
                ? (std::abs(r1 - r2) + std::abs(c1 - c2))
                : (std::numeric_limits<int>::max() / 2);
        }

        static inline bool sameOrder(const Order& a, const Order& b);

        bool shouldIssueNow(const Models::Unit* u, const Order& o, int frameCounter) const;

        void rememberIssued(const Models::Unit* u, const Order& o, int frameCounter);

        void issueOrder(Models::Unit* u, const AI::Order& o, int frameCounter);

        void FightAsWarrior();
    };

    inline bool Commander::sameOrder(const Order& a, const Order& b) {
        if (a.type != b.type) return false;

        switch (a.type) {
        case OrderType::AttackTo: {
            return manhattan(a.row, a.col, b.row, b.col) <= HYST_ATTACK_CELLS;
        }
        case OrderType::DefendAt: {
            return manhattan(a.row, a.col, b.row, b.col) <= HYST_DEFEND_CELLS;
        }
        case OrderType::MoveTo: {
            return manhattan(a.row, a.col, b.row, b.col) <= HYST_MOVE_CELLS;
        }
        case OrderType::Heal:
        case OrderType::Resupply: {
            return (a.targetUnitId == b.targetUnitId);
        }
        default:
            break;
        }
        return (a.row == b.row && a.col == b.col && a.targetUnitId == b.targetUnitId);
    }

} // namespace AI
