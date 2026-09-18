/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

/// Must be before
#include <Scene/Entity.hpp>

#include <Editor/Gizmo/GizmoSwatchOverrideHelpers.hpp>

#include <Scene/Util/SceneHelpers.hpp>

namespace Hyperion {

void ExecuteSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states)
{
    for (const SwatchOverrideTransformEditState& state : states)
    {
        if (!state.entity.IsValid() || (!state.routeToOverride && !state.wasOverridden))
        {
            continue;
        }

        if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*state.entity))
        {
            overrideSystem->SetSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"), BoxedValue(state.postTransform));
        }
    }
}

void RevertSwatchOverrideTransformEdits(const Array<SwatchOverrideTransformEditState>& states)
{
    for (const SwatchOverrideTransformEditState& state : states)
    {
        if (!state.entity.IsValid() || (!state.routeToOverride && !state.wasOverridden))
        {
            continue;
        }

        SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*state.entity);

        if (!overrideSystem)
        {
            continue;
        }

        if (state.wasOverridden)
        {
            overrideSystem->SetSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"), BoxedValue(state.preTransform));
        }
        else
        {
            overrideSystem->RemoveSwatchOverrideValue(state.entity.Get(), state.swatch, NAME("LocalTransform"));
        }
    }
}

} // namespace Hyperion
