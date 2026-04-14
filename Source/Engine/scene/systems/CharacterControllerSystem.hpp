/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <scene/System.hpp>
#include <scene/components/CharacterControllerComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <input/InputHandler.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API CharacterControllerInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(CharacterControllerInputHandler);

public:
    CharacterControllerInputHandler() = default;
    virtual ~CharacterControllerInputHandler() override = default;

    Vec2f GetMovementInput() const;
    bool IsJumpPressed() const;

protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override;
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override;

    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnClick_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnGainFocus_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnLoseFocus_Impl(const MouseEvent& evt) override { return false; }

private:
    float m_forward = 0.0f;
    float m_strafe = 0.0f;
    bool m_jump = false;
};

HYP_CLASS(NoScriptBindings)
class CharacterControllerSystem : public SystemBase
{
    HYP_OBJECT_BODY(CharacterControllerSystem);

public:
    virtual ~CharacterControllerSystem() override = default;

    virtual bool ShouldProcessScene(Scene* scene) const override;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

    virtual bool RequiresSimThread() const override { return true; }
    virtual bool AllowParallelExecution() const override { return false; }

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<CharacterControllerComponent, ComponentAccess::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
