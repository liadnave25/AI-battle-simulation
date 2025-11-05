#pragma once
#include <functional>
#include <vector>
#include <cstdint>

namespace AI {

    enum class EventType : uint8_t {
        EnemySighted,
        LowAmmo,
        Injured,
        UnderFire, 
        OrderIssued,
        OrderAck,
        ReachedTarget,
        NeedResupply,
        CommanderDown
    };

    struct Message {
        EventType type;
        int fromUnitId = -1;
        int toUnitId = -1;
        int row = -1, col = -1;
        int extra = 0;
    };

} // namespace AI