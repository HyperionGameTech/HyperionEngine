#ifndef HYPERION_V2_UI_TEXT_H
#define HYPERION_V2_UI_TEXT_H

#include <ui/UIObject.hpp>
#include <ui/UIScene.hpp>

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Texture.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <math/Transform.hpp>
#include <math/Vector2.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class SystemEvent;
class InputManager;

} // namespace hyperion

namespace hyperion::v2 {

struct UITextOptions
{
    float line_height = 1.0f;
};

class HYP_API UIText : public UIObject
{
public:
    UIText(ID<Entity> entity, UIScene *ui_scene);
    UIText(const UIText &other)                 = delete;
    UIText &operator=(const UIText &other)      = delete;
    UIText(UIText &&other) noexcept             = delete;
    UIText &operator=(UIText &&other) noexcept  = delete;
    virtual ~UIText() override                  = default;

    virtual void Init() override;

    const String &GetText() const
        { return m_text; }

    void SetText(const String &text);

    const RC<FontAtlas> &GetFontAtlas() const
        { return m_font_atlas; }

    void SetFontAtlas(RC<FontAtlas> font_atlas);

    const Vec4f &GetTextColor() const
        { return m_text_color; }

    void SetTextColor(const Vec4f &color);

    const UITextOptions &GetOptions() const
        { return m_options; }

    void SetOptions(const UITextOptions &options)
        { m_options = options; }

protected:
    virtual Handle<Material> GetMaterial() const override;

    virtual void UpdateSize() override;

    void UpdateMesh();

    FontAtlas *GetFontAtlasOrDefault() const;

    String          m_text;
    RC<FontAtlas>   m_font_atlas;

    Vec4f           m_text_color;

    UITextOptions   m_options;
};

} // namespace hyperion::v2

#endif