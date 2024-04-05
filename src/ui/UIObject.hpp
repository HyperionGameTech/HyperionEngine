#ifndef HYPERION_V2_UI_OBJECT_HPP
#define HYPERION_V2_UI_OBJECT_HPP

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <math/Transform.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class UIScene;

// Helper function to get the scene from a UI scene
template <class UISceneType>
static inline Scene *GetScene(UISceneType *ui_scene)
{
    AssertThrow(ui_scene != nullptr);

    return ui_scene->GetScene().Get();
}

// UIObject

enum UIObjectAlignment : uint32
{
    UI_OBJECT_ALIGNMENT_TOP_LEFT        = 0,
    UI_OBJECT_ALIGNMENT_TOP_RIGHT       = 1,

    UI_OBJECT_ALIGNMENT_CENTER          = 2,

    UI_OBJECT_ALIGNMENT_BOTTOM_LEFT     = 3,
    UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT    = 4
};

struct UIObjectSize
{
    using Flags = uint32;

    enum FlagBits : Flags
    {
        RELATIVE = 0x01,
        ABSOLUTE = 0x02,

        GROW     = 0x04,

        PIXEL    = 0x10,
        PERCENT  = 0x20,

        DEFAULT  = ABSOLUTE | PIXEL
    };

    UIObjectSize()
        : flags { DEFAULT, DEFAULT },
          value(0, 0)
    {
    }

    UIObjectSize(Vec2i value)
        : flags { DEFAULT, DEFAULT },
          value(value)
    {
    }

    UIObjectSize(Vec2i value, Flags flags)
        : flags { flags, flags },
          value(value)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by only providing flags. Used primarily for the DYNAMIC type. */
    UIObjectSize(Flags flags)
        : flags { flags, flags },
          value(0, 0)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by providing specific flags for each axis. */
    UIObjectSize(Pair<int, Flags> x, Pair<int, Flags> y)
        : flags { x.second, y.second },
          value(x.first, y.first)
    {
        ApplyDefaultFlags();
    }

    UIObjectSize(const UIObjectSize &other)                 = default;
    UIObjectSize &operator=(const UIObjectSize &other)      = default;
    UIObjectSize(UIObjectSize &&other) noexcept             = default;
    UIObjectSize &operator=(UIObjectSize &&other) noexcept  = default;
    ~UIObjectSize()                                         = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Vec2i &GetValue() const
        { return value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsX() const
        { return flags[0]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsY() const
        { return flags[1]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetAllFlags() const
        { return flags[0] | flags[1]; }

private:
    Flags   flags[2];
    Vec2i   value;

    template <Flags mask>
    HYP_FORCE_INLINE
    void ApplyDefaultFlagMask()
    { 
        for (int i = 0; i < 2; i++) {
            if (!(flags[i] & mask)) {
                flags[i] |= (DEFAULT & mask);
            }
        }
    }

    HYP_FORCE_INLINE
    void ApplyDefaultFlags()
    {
        ApplyDefaultFlagMask<RELATIVE | ABSOLUTE>();
        ApplyDefaultFlagMask<PIXEL | PERCENT>();
    }
};

class UIObject : public EnableRefCountedPtrFromThis<UIObject>
{
public:
    UIObject(ID<Entity> entity, UIScene *parent);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    virtual void Init();

    [[nodiscard]]
    HYP_FORCE_INLINE
    ID<Entity> GetEntity() const
        { return m_entity; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIScene *GetParent() const
        { return m_parent; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsInit() const
        { return m_is_init; }

    Name GetName() const;
    void SetName(Name name);

    Vec2i GetPosition() const;
    void SetPosition(Vec2i position);

    UIObjectSize GetSize() const;
    void SetSize(UIObjectSize size);

    int GetMaxWidth() const;
    void SetMaxWidth(int max_width, UIObjectSize::Flags flags = UIObjectSize::DEFAULT);

    int GetMaxHeight() const;
    void SetMaxHeight(int max_height, UIObjectSize::Flags flags = UIObjectSize::DEFAULT);
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetActualSize() const
        { return m_actual_size; }

    /*! \brief Get the depth of the UI object
     *
     * The depth of the UI object is used to determine the rendering order of the object in the scene, with higher depth values being rendered on top of lower depth values.
     * If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *
     * \return The depth of the UI object
     */
    int GetDepth() const;

    /*! \brief Set the depth of the UI object
     *
     * The depth of the UI object is used to determine the rendering order of the object in the scene, with higher depth values being rendered on top of lower depth values.
     * Set the depth to a value between UIScene::min_depth and UIScene::max_depth. If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *
     * \param depth The depth of the UI object
     */
    void SetDepth(int depth);

    UIObjectAlignment GetOriginAlignment() const;
    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;
    void SetParentAlignment(UIObjectAlignment alignment);

    void AddChildUIObject(UIObject *ui_object);

    NodeProxy GetNode() const;

    BoundingBox GetLocalAABB() const;
    BoundingBox GetWorldAABB() const;

    void UpdatePosition();
    void UpdateSize();

    Delegate<bool, const UIMouseEventData &>    OnMouseDown;
    Delegate<bool, const UIMouseEventData &>    OnMouseUp;
    Delegate<bool, const UIMouseEventData &>    OnMouseDrag;
    Delegate<bool, const UIMouseEventData &>    OnMouseHover;
    Delegate<bool, const UIMouseEventData &>    OnClick;

protected:
    virtual Handle<Material> GetMaterial() const;

    const Handle<Mesh> &GetMesh() const;

    const UIObject *GetParentUIObject() const;

    /*! \brief Get the shared quad mesh used for rendering UI objects. Vertices are in range: 0..1, with the origin at the top-left corner.
     *
     * \return The shared quad mesh
     */
    static Handle<Mesh> GetQuadMesh();

    void UpdateActualSizes();
    void ComputeActualSize(const UIObjectSize &size, Vec2i &out_actual_size, bool clamp = false);

    ID<Entity>          m_entity;
    UIScene             *m_parent;

    Name                m_name;

    Vec2i               m_position;

    UIObjectSize        m_size;
    Vec2i               m_actual_size;

    UIObjectSize        m_max_size;
    Vec2i               m_actual_max_size;

    int                 m_depth; // manually set depth; otherwise defaults to the node's depth in the scene

    UIObjectAlignment   m_origin_alignment;
    UIObjectAlignment   m_parent_alignment;

private:
    bool                m_is_init;

    template <class Lambda>
    void ForEachChildUIObject(Lambda &&lambda);
};

// UIObjectProxy<T>

template <class T>
class UIObjectProxy
{
public:
    UIObjectProxy(NodeProxy node_proxy)
        : m_node_proxy(std::move(node_proxy))
    {
    }

    UIObjectProxy(const UIObjectProxy &other)                   = default;
    UIObjectProxy &operator=(const UIObjectProxy &other)        = default;
    UIObjectProxy(UIObjectProxy &&other) noexcept               = default;
    UIObjectProxy &operator=(UIObjectProxy &&other) noexcept    = default;
    ~UIObjectProxy()                                            = default;

    const NodeProxy &GetNode() const
        { return m_node_proxy; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return m_node_proxy.GetEntity().IsValid(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_node_proxy.GetEntity().IsValid(); }

    [[nodiscard]]
    T *Get() const
    {
        if (!IsValid()) {
            return nullptr;
        }

        Scene *scene = m_node_proxy.Get()->GetScene();
        AssertThrow(scene != nullptr);

        UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(m_node_proxy.GetEntity());
        AssertThrow(ui_component != nullptr && ui_component->ui_object != nullptr);
        AssertThrow(ui_component->ui_object.Is<T>());

        return static_cast<T *>(ui_component->ui_object.Get());
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    operator T*() const
        { return Get(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T *operator->() const
        { return EnsureValidPointer(Get()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T &operator*() const
        { return *EnsureValidPointer(Get()); }

private:
    NodeProxy   m_node_proxy;
};

} // namespace hyperion::v2

#endif