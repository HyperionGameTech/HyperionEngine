#ifndef HYPERION_V2_UI_BUTTON_HPP
#define HYPERION_V2_UI_BUTTON_HPP

#include <ui/UIObject.hpp>

namespace hyperion::v2 {

class UIScene;

// UIButton

class HYP_API UIButton : public UIObject
{
public:
    UIButton(ID<Entity> entity, UIScene *ui_scene, NodeProxy node_proxy);
    UIButton(const UIButton &other)                 = delete;
    UIButton &operator=(const UIButton &other)      = delete;
    UIButton(UIButton &&other) noexcept             = delete;
    UIButton &operator=(UIButton &&other) noexcept  = delete;
    virtual ~UIButton() override                    = default;

protected:
    virtual Handle<Material> GetMaterial() const override;
};

} // namespace hyperion::v2

#endif