/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Pair.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/BoxedValue.hpp>

namespace Hyperion {

class Entity;
class Node;

struct SwatchOverrideTransformEditState
{
    Handle<Entity> entity;
    Name swatch;
    Transform preTransform;
    Transform postTransform;
    
    bool routeToOverride = false; // override mode: edits land in the active swatch's set
    bool wasOverridden = false;   // LocalTransform was overridden in the active swatch's set
};

/*! Captures swatch-override state for entities affected by a gizmo transform edit */
template <class T>
static Array<SwatchOverrideTransformEditState> CaptureSwatchOverrideTransformEdits(
    const Array<Pair<Handle<Node>, T>>& nodeData,
    bool overrideMode)
{
    Array<SwatchOverrideTransformEditState> result;

    for (const auto& pair : nodeData)
    {
        const Handle<Entity>& entity = DynamicCast<Entity>(pair.first);

        if (!entity.IsValid())
        {
            continue;
        }

        SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

        if (!overrideSystem)
        {
            continue;
        }

        World* world = entity->GetWorld();

        if (!world)
        {
            continue;
        }

        const Name activeSwatch = world->GetActiveSwatchName();

        if (!activeSwatch || IsDefaultSwatch(activeSwatch))
        {
            continue;
        }

        const bool hasSet = overrideSystem->HasSwatchOverrideSet(entity, activeSwatch);

        if (overrideMode)
        {
            if (!hasSet)
            {
                overrideSystem->AddSwatchOverrideSet(entity, activeSwatch);
            }

            if (overrideSystem->GetAppliedOverrideSwatch(entity) != activeSwatch)
            {
                overrideSystem->ApplyOverrides(entity, activeSwatch);
            }
        }
        else if (!hasSet)
        {
            continue;
        }

        SwatchOverrideTransformEditState state;
        state.entity = entity;
        state.swatch = activeSwatch;
        state.postTransform = entity->GetLocalTransform();
        state.routeToOverride = overrideMode;
        state.wasOverridden = overrideSystem->IsPropertyOverriddenInSwatch(entity, activeSwatch, NAME("LocalTransform"));

        if (state.wasOverridden)
        {
            BoxedValue preValue;

            if (overrideSystem->GetSwatchOverrideValue(entity, activeSwatch, NAME("LocalTransform"), preValue))
            {
                state.preTransform = preValue.Get<Transform>();
            }
            else
            {
                state.wasOverridden = false;
            }
        }

        if (overrideMode)
        {
            overrideSystem->SetSwatchOverrideValue(entity, activeSwatch, NAME("LocalTransform"), BoxedValue(state.postTransform));
        }

        result.PushBack(std::move(state));
    }

    return result;
}

// Shared execute/revert loops for gizmo undo actions
void ExecuteSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states);
void RevertSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states);

} // namespace Hyperion
