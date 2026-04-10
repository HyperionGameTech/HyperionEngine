#pragma once


#include <Core/FxTypes.hpp>
#include <Renderer/Backend/RxGpuBuffer.hpp>

class RxAnimationManager
{
public:
    RxAnimationManager() = default;

    ~RxAnimationManager() = default;

private:
    RxRawGpuBuffer PoseBuffer;
};
