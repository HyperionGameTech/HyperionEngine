/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>

#include <Core/Memory/AnyRef.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/BitField.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Scene/Node.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/LayerOverrides.hpp>

namespace Hyperion {

class World;
class Scene;
class Node;
class EntityManager;
struct Transform;
struct BoxedValue;
struct RenderProxyMesh;

enum class LayerId : uint32;

struct EntityInitInfo
{
    // Initial tags to add to the Entity when it is created
    FatArray<EntityTag, InlineAllocator<4, SceneAllocator>> initialTags;
    FatArray<Name, InlineAllocator<4, SceneAllocator>> layerNames;
    
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
    explicit Entity(Name name);

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

    //-- Component/Tags --

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

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    void AddTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool RemoveTag();

    template <EntityTag Tag, class EntityManagerPtr = EntityManager*>
    bool HasTag() const;

    //-- Layers --

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasNoLayers() const
    {
        return m_layerMask.CountOnes() == 0;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsInLayer(LayerId layerId) const
    {
        return uint32(layerId) < MaxLayersPerWorld
            && m_layerMask.Test(uint32(layerId));
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

    //-- Layer overrides --

    HYP_FORCE_INLINE bool HasLayerOverrides() const
    {
        return m_layerOverrides.Any();
    }

    const Array<EntityLayerOverrideSet>& GetLayerOverrides() const
    {
        return m_layerOverrides;
    }

    bool HasLayerOverrideSet(Name layerName) const;

    /*! \brief Adds an empty override set for \param layerName. No-op if one already exists. */
    void AddLayerOverrideSet(Name layerName);

    /*! \brief Removes the override set for \param layerName, reverting it first if currently applied.
     *  \returns True if a set was removed. */
    bool RemoveLayerOverrideSet(Name layerName);

    bool IsPropertyOverriddenInLayer(Name layerName, Name propertyName) const;

    /*! \brief Upserts an override value into the set for \param layerName.
     *  If the layer is currently applied, the value is also written to the Entity immediately.
     *  \returns True if the value was set. */
    bool SetLayerOverrideValue(Name layerName, Name propertyName, BoxedValue value);

    /*! \brief Removes an override value from the set for \param layerName.
     *  If the layer is currently applied, the property is restored to its base value.
     *  \returns True if a value was removed. */
    bool RemoveLayerOverrideValue(Name layerName, Name propertyName);

    /*! \brief The Name of the Layer whose overrides are currently applied, or an invalid Name. */
    Name GetAppliedOverrideLayer() const
    {
        return m_appliedOverrideLayer;
    }

    /*! \brief Applies the override set for \param layerName on top of the base property set.
     *  Any currently applied set is reverted first. Base values of all overridden properties
     *  are snapshotted so they can be restored on revert. */
    void ApplyLayerOverrides(Name layerName);

    /*! \brief Reverts any applied overrides, restoring the snapshotted base property values. */
    void RevertLayerOverrides();

    /*! \brief Reads the base value of a property: the snapshotted value if \param layerName is
     *  currently applied, otherwise the live property value.
     *  \returns True if a value was read. */
    bool GetLayerOverrideBaseValue(Name layerName, Name propertyName, BoxedValue& outValue) const;

    /*! \brief Reads a stored override value from the set for \param layerName, without applying it.
     *  \returns True if the property is overridden in that layer. */
    bool GetLayerOverrideValue(Name layerName, Name propertyName, BoxedValue& outValue) const;

    /*! \brief Writes a property's BASE value (through its setter), keeping the applied layer's
     *  base snapshot in sync so a revert returns to the edited base.
     *  \returns True if the property exists, is settable and was written. */
    bool SetLayerOverrideBaseValue(Name propertyName, BoxedValue value);

    /*! \brief Replaces all stored override sets (used by the $LayerOverrides HMF deserializer).
     *  Reverts any applied set first; does not apply. */
    void DeserializeLayerOverrides(Array<EntityLayerOverrideSet>&& sets);

    //-- Tick --

    HYP_METHOD()
    bool ReceivesUpdate() const;

    HYP_METHOD()
    void SetReceivesUpdate(bool receivesUpdate);

    //-- Lock and load --

    virtual void LockTransform() override;
    virtual void UnlockTransform() override;

    //-- Bounds --

    virtual void SetLocalBounds(const BoundingBox& aabb) override;

    //-- RenderProxy --

    void UpdateRenderProxy(RenderProxyMesh* proxy);

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

    //--

protected:
    //-- Overrides --
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

    //--

    EntityInitInfo m_entityInitInfo;

private:
    void SetEntityManager(const Handle<EntityManager>& entityManager);

    //-- Serialization --

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

    //-- Transient properties

    EntityManager* m_entityManager;

    int m_renderProxyVersion;

    // has the transform been updated since the Node's transform has been unlocked?
    bool m_transformChanged : 1;

    HYP_FIELD(Transient)
    BitField<MaxLayersPerWorld> m_layerMask;

    //-- Layer overrides --

    Array<EntityLayerOverrideSet> m_layerOverrides;

    Name m_appliedOverrideLayer;

    // Base values of all overridden properties, captured when a set is applied
    Array<Pair<Name, BoxedValue>> m_overrideBaseSnapshot;

    //--
};

#include <Scene/Entity.inl>

} // namespace Hyperion
