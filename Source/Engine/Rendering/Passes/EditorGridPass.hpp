/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern ENGINE_API CVar<bool> g_cvEditorGrid;

// spacing of the minor grid lines; the editor's translate snapping uses the same grid
extern ENGINE_API CVar<float> g_cvEditorGridSize;

// Y offset moves the grid plane, X/Z offset shift where the lines fall
extern ENGINE_API CVar<float> g_cvEditorGridOffsetX;
extern ENGINE_API CVar<float> g_cvEditorGridOffsetY;
extern ENGINE_API CVar<float> g_cvEditorGridOffsetZ;

class EditorGridPass final : public FullScreenPass
{
public:
    EditorGridPass();

    EditorGridPass(const EditorGridPass& other) = delete;
    EditorGridPass& operator=(const EditorGridPass& other) = delete;

    virtual ~EditorGridPass() override;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderCheckerboarded() const override
    {
        return false;
    }
};

} // namespace Hyperion
