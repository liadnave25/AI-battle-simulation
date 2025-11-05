#pragma once
#include <cstdint>

namespace Definitions {

    constexpr float PI = 3.14159265358979323846f;

    constexpr int GRID_SIZE = 120;    
    constexpr int CELL_PX = 8;       
    constexpr int TICK_MS = 16;    

    enum Cell : int {
        EMPTY = 0,
        ROCK = 1,
        TREE = 2,
        WATER = 3,
        DEPOT_AMMO = 4,
        DEPOT_MED = 5
    };

    enum class Team : uint8_t { Blue = 0, Orange = 1 };
    enum class Role : uint8_t { Commander, Warrior, Medic, Supplier };

    constexpr int   HP_MAX = 100;
    constexpr int   HP_CRITICAL = 40;
    constexpr int   DAMAGE_BULLET = 1;
    constexpr int   DAMAGE_GRENADE = 45;

    constexpr int   AMMO_INIT = 20;
    constexpr int   GRENADE_INIT = 2;
    constexpr int   RELOAD_TIME_TK = 40;

    constexpr int   MEDIC_HEAL_AMOUNT = 60;          
    constexpr int   MEDIC_REFILL_AMOUNT = 100;         
    constexpr int   MEDIC_REFILL_THRESHOLD = 40;       
    constexpr int   SUPPLIER_SUPPLY_AMOUNT = 10;       
    constexpr int   SUPPLIER_REFILL_AMOUNT = 20;       
    constexpr int   SUPPLIER_REFILL_THRESHOLD = 10;      

    constexpr int   MEDIC_REFILL_AMOUNT_FIGHTING = 60;   
    constexpr int   SUPPLIER_REFILL_AMOUNT_FIGHTING = 10; 
    constexpr int   SUPPLIER_SELF_REFILL_THRESHOLD = 0;    
    constexpr int   HP_MED = 50;

    constexpr float BULLET_SPEED = 0.45f;
    constexpr float GRENADE_SPEED = 0.25f;

    constexpr int SIGHT_RANGE = 140;
    constexpr int FIRE_RANGE = 24;
    constexpr int GRENADE_RANGE = 18;

    constexpr int   SECURITY_SAMPLES = 60;
    constexpr float SECURITY_DECAY = 0.985f;
    constexpr float WARRIOR_UNDER_FIRE_THRESHOLD = 0.35f;
    constexpr float TREE_COVER_PENALTY_MULTIPLIER = 1.5f;

    constexpr int PLAY_TOP = 8;
    constexpr int PLAY_BOTTOM = 8;
    constexpr int PLAY_LEFT = 3;
    constexpr int PLAY_RIGHT = 3;

    struct Color { float r, g, b; constexpr Color(float r, float g, float b) : r(r), g(g), b(b) {} };

    constexpr Color COLOR_BG{ 0.72f, 0.86f, 0.62f };
    constexpr Color COLOR_GRID_EMPTY{ 0.90f, 0.90f, 0.90f };
    constexpr Color COLOR_ROCK{ 0.25f, 0.25f, 0.25f };
    constexpr Color COLOR_TREE{ 0.10f, 0.60f, 0.10f };
    constexpr Color COLOR_WATER{ 0.20f, 0.50f, 0.95f };
    constexpr Color COLOR_DEPOT{ 0.95f, 0.85f, 0.20f };

    constexpr Color COLOR_BLUE{ 0.12f, 0.45f, 0.95f };
    constexpr Color COLOR_ORANGE{ 0.95f, 0.55f, 0.12f };

    constexpr int   REPLAN_CHECK_INTERVAL = 2;
    constexpr float REPLAN_RISK_DELTA = 0.18f;
    constexpr int   REPLAN_COOLDOWN_FRAMES = 12;

    constexpr int   LOCAL_COVER_RADIUS = 2;
    constexpr float LOCAL_COVER_DELTA = 0.22f;

    constexpr float ASTAR_RISK_WEIGHT = 5.5f;

    constexpr float DETOUR_MAX_RATIO = 1.8f;
    constexpr float DETOUR_MIN_RISK_DROP = 0.15f;

    constexpr float COMMANDER_SAFE_RISK_MAX = 0.18f;
    constexpr int   COMMANDER_ANCHOR_RETRIES = 400;

}