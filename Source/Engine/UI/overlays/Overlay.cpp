/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/overlays/Overlay.hpp>

#include <Core/threading/Threads.hpp>

#include <Rendering/Texture.hpp>

#include <UI/UIImage.hpp>
#include <UI/UIText.hpp>
#include <UI/UIPanel.hpp>

#include <Overlay.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region OverlayBase

OverlayBase::OverlayBase()
    : m_timer { 0.0333f } // update max. 30hz/s default
{
}

OverlayBase::~OverlayBase()
{
}

void OverlayBase::Initialize(UIObject* spawnParent)
{
    AssertOnThread(g_simThread);
    Assert(spawnParent != nullptr);

    m_uiObject = CreateUIObject(spawnParent);

    if (!m_uiObject)
    {
        return;
    }
}

Handle<UIObject> OverlayBase::CreateUIObject_Impl(UIObject* spawnParent)
{
    return spawnParent->CreateUIObject<UIImage>(InstanceClass()->GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
}

#pragma endregion OverlayBase

#pragma region TextureOverlay

TextureOverlay::TextureOverlay(const Handle<Texture>& texture)
    : m_texture(texture)
{
}

TextureOverlay::~TextureOverlay()
{
    m_onTextureChangeHandle.Reset();
}

void TextureOverlay::SetTexture(const Handle<Texture>& texture)
{
    {
        Mutex::Guard guard(m_textureMtx);
        m_texture = texture;
    }

    OnTextureChange(texture);
}

Handle<UIObject> TextureOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    Vec2u extent = { 500, 250 };

    Mutex::Guard guard(m_textureMtx);
    if (m_texture)
    {
        if (!m_texture->IsCreated())
        {
            CheckResult(m_texture->Create());
        }

        extent = m_texture->GetExtent().GetXY();
    }
;
    const int displayWidth = 500;
    const int displayHeight = (extent.x > 0)
        ? int(float(displayWidth) * float(extent.y) / float(extent.x))
        : displayWidth;

    Handle<UIPanel> panelBackdrop = spawnParent->CreateUIObject<UIPanel>(
        NAME("TextureOverlay_PanelBackdrop"),
        Vec2i(2, 2),
        UIObjectSize({ displayWidth, UIObjectSize::PIXEL }, { displayHeight, UIObjectSize::PIXEL }));

    panelBackdrop->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.8f));
    panelBackdrop->SetPadding(Vec2i::Zero());

    m_image = spawnParent->CreateUIObject<UIImage>(
        InstanceClass()->GetName(),
        Vec2i::Zero(),
        UIObjectSize({ displayWidth, UIObjectSize::PIXEL }, { displayHeight, UIObjectSize::PIXEL }));

    m_image->SetTexture(m_texture);
    m_image->SetAllowMaterialUpdate(true);

    m_onTextureChangeHandle = OnTextureChange
        .BindThreaded([image = m_image](const Handle<Texture>& texture)
        {
            image->SetTexture(texture);
        },
        g_simThread);

    panelBackdrop->AddChildUIObject(m_image);

    return panelBackdrop;
}

#pragma endregion TextureOverlay

#pragma region TextOverlay

TextOverlay::TextOverlay(const String& text, Color textColor, float textSize)
    : m_text(text),
      m_textColor(textColor),
      m_textSize(textSize)
{
}

TextOverlay::~TextOverlay()
{
}

Handle<UIObject> TextOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    Handle<UIText> uiText = spawnParent->CreateUIObject<UIText>(InstanceClass()->GetName(), Vec2i::Zero(), UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
    uiText->SetText(m_text);
    uiText->SetTextColor(m_textColor);
    uiText->SetTextSize(m_textSize);
    uiText->SetPadding(Vec2i { 2, 2 });

    return uiText;
}

#pragma endregion TextOverlay

} // namespace Hyperion
