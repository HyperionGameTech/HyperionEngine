#pragma once

#include <Core/FxTypes.hpp>

enum RxShaderName : uint32
{
    eGeometry,    // 0
    eLighting,    // 1
    eComposition, // 2
    eShadows,     // 3
    eUnlit,       // 4

    eNumShaders,
};

namespace RxShaderNameUtil {


constexpr uint32 scNumShaders = static_cast<uint32>(eNumShaders);

constexpr const char* GetName(const RxShaderName id)
{
    switch (id) {
    case eGeometry:
        return "Geometry";
    case eLighting:
        return "Lighting";
    case eComposition:
        return "Composition";
    case eShadows:
        return "Shadows";
    case eUnlit:
        return "Unlit";
    case eNumShaders:
        break;
    }
    return nullptr;
}

} // namespace RxShaderNameUtil
