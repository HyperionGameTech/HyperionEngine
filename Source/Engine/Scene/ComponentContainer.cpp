/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ComponentContainer.hpp>
#include <Scene/ComponentInterface.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <ComponentContainer.generated.inl>

namespace Hyperion {

// matches the page size the typed SparseArray storage used to target
static constexpr size_t g_componentSlabTargetSize = 64 * 1024;
static constexpr size_t g_maxComponentSize = 1024 * 1024;

static size_t CalculateComponentBlocksPerSlab(size_t componentSize, size_t componentAlignment)
{
    const size_t minimumBlockSize = sizeof(void*) > sizeof(uint32) ? sizeof(void*) : sizeof(uint32);
    const size_t alignment = componentAlignment > alignof(void*) ? componentAlignment : alignof(void*);
    const size_t blockSize = ByteUtil::AlignAs(componentSize > minimumBlockSize ? componentSize : minimumBlockSize, uint32(alignment));
    const size_t blocksPerSlab = g_componentSlabTargetSize / blockSize;

    return blocksPerSlab != 0 ? blocksPerSlab : 1;
}

static void CopyMatchingProperties(const Struct& sourceStruct, const BoxedValue& source, const Struct& targetStruct, BoxedValue& target)
{
    for (const Property* sourceProperty : sourceStruct.GetProperties())
    {
        if (!sourceProperty->CanGet())
        {
            continue;
        }

        const Property* targetProperty = targetStruct.GetProperty(sourceProperty->GetName(), /* deep */ false);

        if (!targetProperty || !targetProperty->CanSet() || targetProperty->GetTypeInfo().id != sourceProperty->GetTypeInfo().id)
        {
            continue;
        }

        targetProperty->Set(target, sourceProperty->Get(source));
    }
}

ComponentContainer::ComponentContainer(const ComponentInterface& componentInterface)
    : m_componentInterface(&componentInterface),
      m_typeInfo(&componentInterface.GetTypeInfo()),
      m_typeId(componentInterface.GetTypeId()),
      m_structReference(componentInterface.GetStruct()),
      m_components(
          componentInterface.GetComponentSize(),
          componentInterface.GetComponentAlignment(),
          CalculateComponentBlocksPerSlab(componentInterface.GetComponentSize(), componentInterface.GetComponentAlignment())),
      m_numComponents(0)
{
    Assert(componentInterface.GetComponentSize() != 0, "Component type {} has no size", m_typeId.Value());
    Assert(componentInterface.GetComponentSize() <= g_maxComponentSize, "Component type {} is too large ({} bytes)", m_typeId.Value(), componentInterface.GetComponentSize());
}

ComponentContainer::~ComponentContainer()
{
    // only the Struct (kept alive by m_structReference) is touched here; TypeInfo and the registry may already be gone at shutdown
    m_components.ForEachElementRaw([this](size_t, ubyte* component)
        {
            m_componentInterface->DestructComponent(component);
        });

    m_components.Reset();
}

void* ComponentContainer::AllocateComponentSlot(ComponentId& outId)
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    outId = ComponentId(m_componentIdAllocator.Allocate());
    Assert(outId != Invalid<ComponentId>, "Out of component ids for component type {}", m_typeId.Value());

    ++m_numComponents;

    return m_components.AllocateElementRaw(uint32(outId));
}

void ComponentContainer::FreeComponentSlot(ComponentId id)
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    m_components.FreeElementRaw(uint32(id));
    m_componentIdAllocator.Free(uint32(id));

    AssertDebug(m_numComponents != 0);
    --m_numComponents;
}

ComponentId ComponentContainer::AddDefaultComponent()
{
    ComponentId id;
    void* memory = AllocateComponentSlot(id);

    m_componentInterface->ConstructComponent(memory);

    return id;
}

ComponentId ComponentContainer::AddComponentCopy(const void* source)
{
    Assert(source != nullptr, "Cannot add a component from a null source");

    ComponentId id;
    void* memory = AllocateComponentSlot(id);

    m_componentInterface->CopyConstructComponent(memory, source);

    return id;
}

ComponentId ComponentContainer::AddComponentMove(void* source)
{
    Assert(source != nullptr, "Cannot add a component from a null source");

    ComponentId id;
    void* memory = AllocateComponentSlot(id);

    m_componentInterface->MoveConstructComponent(memory, source);

    return id;
}

ComponentId ComponentContainer::AddComponent(const BoxedValue& componentData)
{
    Assert(componentData.IsValid(), "Cannot add an invalid component");
    Assert(componentData.GetTypeId() == m_typeId, "Component data is not of the correct type");

    return AddComponentCopy(componentData.ToRef().GetPointer());
}

ComponentId ComponentContainer::AddComponent(BoxedValue&& componentData)
{
    Assert(componentData.IsValid(), "Cannot add an invalid component");
    Assert(componentData.GetTypeId() == m_typeId, "Component is not of the correct type");

    return AddComponentMove(componentData.ToRef().GetPointer());
}

bool ComponentContainer::RemoveComponent(ComponentId id)
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (id == Invalid<ComponentId>)
    {
        return false;
    }

    void* component = m_components.GetElementRaw(uint32(id));

    if (!component)
    {
        return false;
    }

    m_componentInterface->DestructComponent(component);

    FreeComponentSlot(id);

    return true;
}

bool ComponentContainer::RemoveComponent(ComponentId id, BoxedValue& outBoxed)
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (id == Invalid<ComponentId>)
    {
        return false;
    }

    void* component = m_components.GetElementRaw(uint32(id));

    if (!component)
    {
        return false;
    }

    const bool boxed = m_componentInterface->GetStruct()->MoveConstructBoxed(component, outBoxed, m_typeInfo);
    Assert(boxed, "Failed to move component of type {} into a BoxedValue", m_typeId.Value());

    m_componentInterface->DestructComponent(component);

    FreeComponentSlot(id);

    return true;
}

void ComponentContainer::MigrateTo(const ComponentInterface& componentInterface)
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    Assert(componentInterface.GetTypeId() == m_typeId, "Cannot migrate components of type {} to type {}", m_typeId.Value(), componentInterface.GetTypeId().Value());
    Assert(componentInterface.GetComponentSize() != 0 && componentInterface.GetComponentSize() <= g_maxComponentSize,
        "Component type {} has an invalid size ({} bytes)", m_typeId.Value(), componentInterface.GetComponentSize());

    if (&componentInterface == m_componentInterface)
    {
        return;
    }

    const Struct* previousStruct = m_componentInterface->GetStruct();
    const Struct* currentStruct = componentInterface.GetStruct();

    // the old values leave the storage first so it can be rebuilt at the new size.
    // each box keeps the old Struct alive until its values are copied
    Array<Pair<ComponentId, BoxedValue>> previousComponents;
    previousComponents.Reserve(m_numComponents);

    m_components.ForEachElementRaw([&](size_t index, ubyte* component)
        {
            BoxedValue previousComponent;

            const bool boxed = previousStruct->MoveConstructBoxed(component, previousComponent);
            Assert(boxed, "Failed to move component of type {} into a BoxedValue", m_typeId.Value());

            m_componentInterface->DestructComponent(component);

            previousComponents.EmplaceBack(ComponentId(index), std::move(previousComponent));
        });

    using ComponentStorage = StridedBuffer<SceneAllocator>;

    m_components.Reset();
    m_components.~ComponentStorage();

    new (&m_components) ComponentStorage(
        componentInterface.GetComponentSize(),
        componentInterface.GetComponentAlignment(),
        CalculateComponentBlocksPerSlab(componentInterface.GetComponentSize(), componentInterface.GetComponentAlignment()));

    m_componentInterface = &componentInterface;
    m_typeInfo = &componentInterface.GetTypeInfo();
    m_structReference = ClassRef(currentStruct);

    for (Pair<ComponentId, BoxedValue>& previousComponent : previousComponents)
    {
        void* component = m_components.AllocateElementRaw(uint32(previousComponent.first));

        m_componentInterface->ConstructComponent(component);

        BoxedValue target(AnyRef(m_typeInfo, component));
        CopyMatchingProperties(*previousStruct, previousComponent.second, *currentStruct, target);
    }
}

} // namespace Hyperion
