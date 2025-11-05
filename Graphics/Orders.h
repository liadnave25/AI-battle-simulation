#pragma once
#include <cstdint>

namespace AI {

    enum class OrderType : uint8_t {
        MoveTo,    
        AttackTo,   
        DefendAt,   
        Resupply,   
        Heal       
    };

    struct Order {
        OrderType type{};
        int row = -1, col = -1;     
        int targetUnitId = -1;     
    };

} // namespace AI
