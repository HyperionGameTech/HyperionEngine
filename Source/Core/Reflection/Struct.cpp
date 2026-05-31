/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Field.hpp>

#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#endif

namespace Hyperion {

static void* DynamicStructInstance_CopyCtor(void* ctx, const void* block)
{
    const Any::Block* src = static_cast<const Any::Block*>(block);
    const DynamicStructInstance* pStruct = static_cast<const DynamicStructInstance*>(ctx);

    void* objCopy = pStruct->GetFunctions().copy(const_cast<void*>(ctx), src->objectPtr);

    static auto* s_allocator = GetDefaultAllocatorInstance<DynamicAllocator>();

    void* raw = s_allocator->Allocate(sizeof(Any::Block), alignof(Any::Block));
    HYP_CORE_ASSERT(raw != nullptr);

    Any::Block* hdr = new (raw) Any::Block {
        src->typeInfo,
        objCopy,
        ctx,
        &DynamicStructInstance_CopyCtor,
        pStruct->GetFunctions().destruct,
        src->dtor,
        src->objSize,
        src->objAlign
    };

    return hdr;
}

#pragma region Struct

bool Struct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, size_t size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        size_t size;
    };

    AssertDebug(objectPtr != nullptr);

#ifdef HYP_DOTNET
    if (dotnet::ManagedClass* managedClass = Class::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        ScriptObjectFunctions::ManagedClassNewManagedObject(managedClass, &context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                AssertDebug(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %zu but got %u",
                    context.size, objectSize);

                Memory::Copy(objectPtr, context.ptr, context.size);
            }, &outObjectReference);

        return true;
    }
#endif

    return false;
}

#pragma endregion Struct

#pragma region DynamicStructInstance

#ifdef HYP_SCRIPT
DynamicStructInstance::DynamicStructInstance(
    TypeId typeId,
    Name name,
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<MemberVariant> members,
    const DynamicStructInstanceFunctions& functions)
    : Struct(typeId, name, -1, 0, Name::Invalid(), attributes, flags | ClassFlags::DYNAMIC, members),
      m_functions(functions)
{
    m_refCount = 0;
    size_t dynamicSize = 0;
    size_t dynamicAlignment = 0;

    auto CalculateDynamicClassSize = [](const Class* cls, size_t& dynamicSize, size_t& dynamicAlignment)
    {
        AssertDebug(cls->IsDynamic());

        for (const Field* field : cls->GetFields())
        {
            // In dynamic classes for scripts, all fields are stored as BoxedValue
            const size_t fieldSize = sizeof(BoxedValue);
            const size_t fieldAlignment = alignof(BoxedValue);

            dynamicSize = ByteUtil::AlignAs(dynamicSize, fieldAlignment);

            AssertDebug(field != nullptr);
            AssertDebug(field->GetOffset() == dynamicSize, "Field offsets don't match expected offset! (field: {}, class: {}), expected {}, got {}",
                field->GetName(), cls->GetName(),
                dynamicSize, field->GetOffset());

            dynamicSize += fieldSize;

            dynamicAlignment = MathUtil::Max(dynamicAlignment, fieldAlignment);
        }
    };

    CalculateDynamicClassSize(this, dynamicSize, dynamicAlignment);

    m_size = MathUtil::Max(1, dynamicSize);
    m_alignment = MathUtil::Max(1, dynamicAlignment);
}
#endif

DynamicStructInstance::DynamicStructInstance(
    TypeId typeId,
    Name name,
    uint32 size,
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<MemberVariant> members,
    const DynamicStructInstanceFunctions& functions)
    : Struct(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members),
      m_functions(functions)
{
    m_refCount = 0;
    Assert(size > 0);

    m_size = size;
    m_alignment = alignof(void*);

    /// \todo Register the ManagedClass (dotnet::ManagedClass) for this. We need the assembly.
    ClassRegistry::GetInstance().Register(typeId, this);
}

DynamicStructInstance::~DynamicStructInstance()
{
    Assert(AtomicAdd(&m_refCount, 0) <= 0, "DynamicStructInstance destroyed while still being referenced!");
}

#ifdef HYP_DOTNET
bool DynamicStructInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    Assert(objectPtr != nullptr);

    // Construct a new instance of the struct and return an ObjectReference pointing to it.
    if (!CreateStructInstance(outObjectReference, objectPtr, m_size))
    {
        return false;
    }

    return true;
}
#endif

bool DynamicStructInstance::ToBoxed(ByteView memory, BoxedValue& out) const
{
    void* data = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(m_size, m_alignment);
    Assert(data != nullptr);

    Memory::Copy(data, memory.Data(), m_size);

    const TypeInfo* pTypeInfo = GetTypeInfo();
    AssertDebug(pTypeInfo != nullptr);

    out = BoxedValue(Any::FromVoidPointer<DynamicAllocator>(pTypeInfo, data, &DynamicStructInstance_CopyCtor, m_functions.destruct, const_cast<void*>(static_cast<const void*>(this)), m_size, m_alignment));

    return true;
}

bool DynamicStructInstance::CreateInstance_Internal(BoxedValue& out) const
{
    void* data = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(m_size, m_alignment);
    Assert(data != nullptr);

    AssertDebug(m_functions.construct != nullptr);
    m_functions.construct(const_cast<void*>(static_cast<const void*>(this)), data);

    const TypeInfo* pTypeInfo = GetTypeInfo();
    AssertDebug(pTypeInfo != nullptr);

    out = BoxedValue(Any::FromVoidPointer<DynamicAllocator>(pTypeInfo, data, &DynamicStructInstance_CopyCtor, m_functions.destruct, const_cast<void*>(static_cast<const void*>(this)), m_size, m_alignment));

    return true;
}

bool DynamicStructInstance::CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const
{
    HYP_NOT_IMPLEMENTED();

    return false;
}

void DynamicStructInstance::AddRef()
{
    AtomicIncrement(&m_refCount);
}

void DynamicStructInstance::Release()
{
    if (AtomicDecrement(&m_refCount) <= 0)
    {
        if (!ClassRegistry::GetInstance().Unregister(this))
        {
            HYP_LOG(Object, Warning, "Failed to unregister dynamic Struct \"{}\"", GetName());
        }

        delete this;
    }
}

#pragma endregion DynamicStructInstance

} // namespace Hyperion
