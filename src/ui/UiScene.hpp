#ifndef HYPERION_V2_UI_SCENE_H
#define HYPERION_V2_UI_SCENE_H

#include <rendering/Base.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <vector>

namespace hyperion::v2 {

using renderer::Extent2D;

class UIObject : public EngineComponentBase<STUB_CLASS(UIObject)> {
public:
    UIObject() : m_position{}, m_dimensions{} {}
    virtual ~UIObject() = default;

    const Extent2D &GetPosition() const   { return m_position; }
    const Extent2D &GetDimensions() const { return m_dimensions; }

protected:
    Extent2D m_position,
             m_dimensions;
};

class UIScene {
public:

    template <class T, class ...Args>
    UIObject *Add(Args &&...args)
    {
        static_assert(std::is_base_of_v<UIObject, T>, "T must be a derived class of UIObject");

        return AddUIObject(std::make_unique<T>(std::forward<Args>(args)...));    
    }

private:
    UIObject *AddUIObject(std::unique_ptr<UIObject> &&ui_object);

    std::vector<Ref<UIObject>> m_ui_objects;
};

} // namespace hyperion::v2

#endif