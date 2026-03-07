#pragma once
#include <any>

template <typename Canvas, typename IVPSPtr>
class DrawStrategy {
public:
    virtual ~DrawStrategy() = default;
    virtual void draw(const std::any& data, Canvas& canvas, IVPSPtr ivps) = 0;
};
