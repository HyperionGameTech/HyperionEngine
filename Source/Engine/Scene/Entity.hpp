/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>

#include <Core/Memory/AnyRef.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/BitField.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Scene/Node.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Layer.hpp>
#include <Scene/Components/SwatchOverridesComponent.hpp>

namespace Hyperion {

class World;
class Scene;
class Node;
class EntityManager;
struct Transform;
struct BoxedValue;
struct RenderProxyMesh;

enum class LayerId : uint32;

using EntityUnresolvedComponents = Array<HMF::UnresolvedObject, SceneAllocator>;
using EntitySwatchOverrideSets = Array<EntitySwatchOverrideSet, SceneAllocator>;

struct EntityInitInfo
{
    // Initial tags to add to the Entity when it is created
    Array<EntityTag, SceneAllocator> initialTags;
    Array<Name, SceneAllocator> layerNames;

    EntitySwatchOverrideSets* pendingSwatchOverrides = nullptr;
    EntityUnresolvedComponents* unresolvedComponents = nullptr;
    
    bool receivesUpdate = false;
    bool canEverUpdate = true;

    uint8 bvhDepth = 3; // 0 means no BVH, 1 means 1 level deep, etc.
};

HYP_CLASS()
class ENGINE_API Entity : public Node
{
    HYP_OBJECT_BODY(Entity);

public:
    friend class EntityManager;
    friend class Node;

    Entity();
    explicit Entity(Name name, const EntityInitInfo& initInfo = {});

    virtual ~Entity() override;

    /*! \brief Clone this entity, including all components.
     *  \returns A handle to the cloned entity. */
    HYP_METHOD()
    virtual Handle<Node> Clone() const override;

    HYP_METHOD()
    HYP_FORCE_INLINE EntityManager* GetEntityManager() const
    {
        return m_entityManager;
    }

    /*! \brief Does this entity hold saved components whose class isn't registered (yet)? */
    HYP_FORCE_INLINE bool HasUnresolvedComponents() const
    {
        return m_entityInitInfo.unresolvedComponents
            && m_entityInitInfo.unresolvedComponents->Any();
    }

    ///Component/Tags

    template <class Component, class EntityManagerPtr = EntityManager*>
    Component& GetComponent() const;

    template <class Component, class EntityManagerPtr = EntityManager*>
    Component* TryGetComponent() const;

    template <class Component, class EntityManagerPtr = EntityManager*>
    bool HasComponent() const;

    template <class Component, class T = Component, class EntityManagerPtr = EntityManager*>
    Component& AddComponent(T&& component);

    template <class Component, class EntityManagerPtr = EntityManager*>
    bool RemoveComponent();

    /*! \brief Check whether this Entity has a MeshComponent. */
    HYP_METHOD()
    bool IsMeshEntity() const;

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    void AddTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool RemoveTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool HasTag() const;

    ///Layers

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasNoLayers() const
    {
        return m_layersMask.CountOnes() == 0;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsInLayer(LayerId layerId) const
    {
        return uint32(layerId) < MaxLayersPerWorld
            && m_layersMask.Test(uint32(layerId));
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsInAnyLayers(const LayersMask& layerIds) const
    {
        return (m_layersMask & layerIds).CountOnes();
    }

    HYP_METHOD()
    void AddToLayer(LayerId layerId);

    HYP_METHOD()
    void RemoveFromLayer(LayerId layerId);

    HYP_METHOD()
    bool IsInLayerByName(Name layerName) const;

    HYP_METHOD()
    void AddToLayerByName(Name layerName);

    HYP_METHOD()
    void RemoveFromLayerByName(Name layerName);

    ///Swatch overrides

    void SetPendingSwatchOverrides(Array<EntitySwatchOverrideSet>&& sets);
    void FlushPendingSwatchOverrides();

    ///Tick

    HYP_METHOD()
    bool ReceivesUpdate() const;

    HYP_METHOD()
    void SetReceivesUpdate(bool receivesUpdate);

    ///Lock and load

    virtual void LockTransform() override;
    virtual void UnlockTransform() override;

    ///Bounds

    virtual void SetLocalBounds(const BoundingBox& aabb) override;

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Fit Bounds to Mesh", EditCondition = "IsMeshEntity")
    void FitBoundsToMesh();
#endif

    ///RenderProxy

    void UpdateRenderProxy(RenderProxyMesh* proxy);

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

    ////////////////////

protected:
    ///Overrides
    virtual void Init() override;

    virtual void Update(float delta)
    {
    }

    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnNodeAttached(Node* node) override;
    virtual void OnNodeDetached(Node* node) override;

    virtual void OnAddedToWorld(World* world);
    virtual void OnRemovedFromWorld(World* world);

    virtual void OnAddedToScene(Scene* scene);
    virtual void OnRemovedFromScene(Scene* scene);

    virtual void OnComponentAdded(AnyRef component);
    virtual void OnComponentRemoved(AnyRef component);

    virtual void OnTagAdded(EntityTag tag);
    virtual void OnTagRemoved(EntityTag tag, bool refreshDependentTags = true);

    virtual void SetScene_Internal(Scene* scene, bool moveToDetached) override;

    virtual void OnTransformUpdated() override;
    virtual void OnMobilityChanged(bool isStatic) override;

    ////////////////////

    EntityInitInfo m_entityInitInfo;

private:
    void SetEntityManager(const Handle<EntityManager>& entityManager);
    void SetEntityManagerRaw_Internal(EntityManager* entityManager);

    ///Serialization

    HYP_METHOD(Property = "Tags", NoScriptBindings)
    Array<Name> SerializeTags() const;

    HYP_METHOD(Property = "Tags", NoScriptBindings, LoadOrder = 1001)
    void DeserializeTags(const Array<Name>& tags);

    HYP_METHOD(Property = "Components", NoScriptBindings)
    Array<BoxedValue, DynamicAllocator> SerializeComponents() const;

    HYP_METHOD(Property = "Components", NoScriptBindings, LoadOrder = 1000)
    void DeserializeComponents(const Array<BoxedValue, DynamicAllocator>& components);

    HYP_METHOD(Property = "Layers", NoScriptBindings)
    Array<Name> SerializeLayers() const;

    HYP_METHOD(Property = "Layers", NoScriptBindings, LoadOrder = 1002)
    void DeserializeLayers(const Array<Name>& layerNames);

    ////////////////////

    /*! \brief Re-adds stored unresolved components whose class has since been registered.
     *  Must be called on the EntityManager's owner thread while it is unlocked.
     *  \return True if unresolved components remain. */
    bool ResolveUnresolvedComponents();

    ///Transient properties

    EntityManager* m_entityManager;

    int m_renderProxyVersion;

    // has the transform been updated since the Node's transform has been unlocked?
    bool m_transformChanged : 1;

    HYP_FIELD(Transient)
    LayersMask m_layersMask;

    ////////////////////
};

#include <Scene/Entity.inl>

} // namespace Hyperion
