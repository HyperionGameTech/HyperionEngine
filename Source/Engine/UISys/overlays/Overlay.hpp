/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/math/Color.hpp>

#include <Core/utilities/ClockTimer.hpp>

#include <Core/Defines.hpp>

#include <ui/UIObject.hpp>

namespace Hyperion {

class UIStage;
class UIImage;

class Texture;

HYP_CLASS(Abstract)
class ENGINE_API OverlayBase : public ObjectBase
{
    HYP_OBJECT_BODY(OverlayBase);

public:
    OverlayBase();
    virtual ~OverlayBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<UIObject>& GetUIObject() const
    {
        return m_uiObject;
    }

    HYP_FORCE_INLINE ClockTimer& GetTimer()
    {
        return m_timer;
    }

    void Initialize(UIObject* spawnParent);

    HYP_METHOD(Scriptable)
    int GetPlacement() const; // 0 = top-left, 1 = bottom-left, 2 = top-right, 3 = bottom-right

    HYP_METHOD(Scriptable)
    void Update(float delta);

    HYP_METHOD(Scriptable)
    Handle<UIObject> CreateUIObject(UIObject* spawnParent);

    HYP_METHOD(Scriptable)
    bool IsEnabled() const;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent);

    HYP_METHOD()
    virtual int GetPlacement_Impl() const
    {
        return 0; // Default to top-left
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
    }

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const
    {
        return true;
    }

    Handle<UIObject> m_uiObject;
    ClockTimer m_timer;
};

HYP_CLASS()
class ENGINE_API TextureOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(TextureOverlay);

public:
    TextureOverlay(const Handle<Texture>& texture);
    virtual ~TextureOverlay() override;

    Handle<Texture> GetTexture() const
    {
        Mutex::Guard guard(m_textureMtx);
        return m_texture;
    }

    void SetTexture(const Handle<Texture>& texture);

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const
    {
        return 2;
    }

    Delegate<void, Handle<Texture>> OnTextureChange;
    DelegateHandler m_onTextureChangeHandle;

    Handle<Texture> m_texture;
    mutable Mutex m_textureMtx;

    Handle<UIImage> m_image;
};

HYP_CLASS()
class ENGINE_API TextOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(TextOverlay);

public:
    TextOverlay(const String& text, Color textColor = Color::White(), float textSize = 10.0f);
    virtual ~TextOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    String m_text;
    Color m_textColor;
    float m_textSize;
};

HYP_CLASS()
class ENGINE_API NullOverlay final : public OverlayBase
{
    HYP_OBJECT_BODY(NullOverlay);

public:
    NullOverlay() = default;
    ~NullOverlay() override = default;

protected:
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override
    {
        return Handle<UIObject>::Null();
    }
};

} // namespace Hyperion
