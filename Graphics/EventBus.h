#pragma once
#include "AIEvents.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace AI {

    class EventBus {
    public:
        using Handler = std::function<void(const Message&)>;

        static EventBus& instance() {
            static EventBus bus; return bus;
        }

        int subscribe(int key, Handler h) {
            handlers[key].push_back(std::move(h));
            return (int)handlers[key].size();
        }

        void publish(const Message& m) {
            if (m.toUnitId != -1) {
                auto it = handlers.find(m.toUnitId);
                if (it != handlers.end()) {
                    for (auto& h : it->second) h(m);
                }
                return;
            }

            auto it = handlers.find(-1);
            if (it != handlers.end()) {
                for (auto& h : it->second) h(m);
            }
        }


    private:
        std::unordered_map<int, std::vector<Handler>> handlers;
    };

} // namespace AI
