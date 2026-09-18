/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Utilities/Result.hpp>

#include <Physics/PhysicsShape.hpp>

namespace Hyperion {

class Mesh;

struct ConvexDecompositionResult
{
    Array<float> positions;
    Array<uint32> indices;

    Array<ConvexHullRange> hulls;
};

ConvexDecompositionSettings GetConvexDecompositionPreset(uint32 presetIndex);

uint32 GetNumConvexDecompositionPresets();

const char* GetConvexDecompositionPresetName(uint32 presetIndex);

bool IsConvexDecompositionSupported();

TResult<ConvexDecompositionResult> DecomposeMesh(
    const Mesh* mesh,
    const ConvexDecompositionSettings& settings,
    const Proc<bool(float)>& progressCallback = {});

} // namespace Hyperion
