#pragma once
#include <map>
#include <string>
#include <memory>
#include <functional>
#include "draw_strategy.hpp"

template <typename Canvas, typename IVPSPtr>
class DrawRegistry {
public:
    using DrawerCreator = std::function<std::shared_ptr<DrawStrategy<Canvas, IVPSPtr>>()>;

    static DrawRegistry& instance() {
        static DrawRegistry inst;
        return inst;
    }

    void register_drawer(const std::string& type, DrawerCreator func) {
        drawers_[type] = func;
    }

    std::shared_ptr<DrawStrategy<Canvas, IVPSPtr>> get(const std::string& type) {
        if (drawers_.count(type)) {
            return drawers_[type]();
        }
        return nullptr;
    }

private:
    std::map<std::string, DrawerCreator> drawers_;
};

#define REGISTER_DRAW_STRATEGY(StrategyClass, type_str, Canvas, IVPSPtr) \
    static bool _##StrategyClass##_reg = [] { \
        DrawRegistry<Canvas, IVPSPtr>::instance().register_drawer(type_str, []() { \
            return std::make_shared<StrategyClass>(); \
        }); \
        return true; \
    }()
