/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

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
