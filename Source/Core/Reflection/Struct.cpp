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

static void Struct_AddRefIfDynamic(const Struct* pStruct)
{
    if (pStruct->IsDynamic())
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(pStruct))->AddRef();
    }
}

static void Struct_ReleaseIfDynamic(const Struct* pStruct)
{
    if (pStruct->IsDynamic())
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(pStruct))->Release();
    }
}

static void StructBox_DestroyObject(void* context, void* object)
{
    const Struct* pStruct = static_cast<const Struct*>(context);

    pStruct->DestructInPlace(object);
    GetDefaultAllocatorInstance<DynamicAllocator>()->Free(object);

    Struct_ReleaseIfDynamic(pStruct);
}

static void* StructBox_CopyBlock(void* context, const void* sourceBlock)
{
    const Any::Block* source = static_cast<const Any::Block*>(sourceBlock);
    const Struct* pStruct = static_cast<const Struct*>(context);

    void* object = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(pStruct->GetSize(), pStruct->GetAlignment());
    HYP_CORE_ASSERT(object != nullptr);

    pStruct->CopyConstructInPlace(object, source->objectPtr);

    // the new block keeps its own reference on the struct
    Struct_AddRefIfDynamic(pStruct);

    void* raw = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(sizeof(Any::Block), alignof(Any::Block));
    HYP_CORE_ASSERT(raw != nullptr);

    return new (raw) Any::Block {
        source->typeInfo,
        object,
        context,
        source->copyCtor,
        source->objectDtor,
        source->dtor,
        source->objSize,
        source->objAlign
    };
}

#pragma region Struct

void* Struct::AllocateBoxedObject() const
{
    HYP_CORE_ASSERT(GetSize() != 0, "Struct %s has no size", GetName().LookupString());

    void* object = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(GetSize(), GetAlignment() != 0 ? GetAlignment() : alignof(void*));
    HYP_CORE_ASSERT(object != nullptr);

    return object;
}

void Struct::MakeBoxFromObject(void* object, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    if (!boxedTypeInfo)
    {
        boxedTypeInfo = GetTypeInfo();
    }

    HYP_CORE_ASSERT(boxedTypeInfo != nullptr);

    // the box keeps a reference on this struct; released in StructBox_DestroyObject
    Struct_AddRefIfDynamic(this);

    outBoxed = BoxedValue(Any::FromVoidPointer<DynamicAllocator>(
        boxedTypeInfo,
        object,
        CanCopyConstructInPlace() ? &StructBox_CopyBlock : nullptr,
        &StructBox_DestroyObject,
        const_cast<void*>(static_cast<const void*>(this)),
        GetSize(),
        GetAlignment()));
}

bool Struct::ConstructBoxed(BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    if (!CanConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    ConstructInPlace(object);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

bool Struct::CopyConstructBoxed(const void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    HYP_CORE_ASSERT(source != nullptr);

    if (!CanCopyConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    CopyConstructInPlace(object, source);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

bool Struct::MoveConstructBoxed(void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    HYP_CORE_ASSERT(source != nullptr);

    if (!CanMoveConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    MoveConstructInPlace(object, source);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

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


DynamicStructInstance::DynamicStructInstance(
    TypeId typeId,
    Name name,
    uint32 size,
    uint32 alignment,
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<MemberVariant> members,
    const DynamicStructInstanceFunctions& functions)
    : Struct(typeId, name, -1, 0, Name::Invalid(), attributes, flags | ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC, members),
      m_functions(functions)
{
    // starts at 1 for the caller that created it (released via Struct_DestroyDynamicStruct);
    // boxed instances of this struct take their own references on top of that.
    m_refCount = 1;

    Assert(size > 0);
    Assert(size <= UINT16_MAX, "Dynamic struct size {} exceeds TypeInfo limit", size);
    Assert(alignment != 0 && (alignment & (alignment - 1)) == 0, "Dynamic struct alignment {} must be a power of two", alignment);

    m_size = size;
    m_alignment = alignment;

    // the TypeInfo was built by the Class constructor before size/alignment were known; we own it for dynamic types
    TypeInfo* pTypeInfo = const_cast<TypeInfo*>(GetTypeInfo());
    Assert(pTypeInfo != nullptr);

    pTypeInfo->size = uint16(size);
    pTypeInfo->alignment = uint16(alignment);

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
    Assert(memory.Size() >= m_size);

    return MoveConstructBoxed(memory.Data(), out);
}

bool DynamicStructInstance::CreateInstance_Internal(BoxedValue& out) const
{
    return ConstructBoxed(out);
}

void DynamicStructInstance::SetDefaultValue(const void* defaultValue)
{
    if (!defaultValue)
    {
        m_defaultValue = ByteBuffer();

        return;
    }

    m_defaultValue = ByteBuffer(m_size, defaultValue);
}

void DynamicStructInstance::ConstructInPlace(void* destination) const
{
    if (m_functions.construct != nullptr)
    {
        m_functions.construct(GetFunctionContext(), destination);

        return;
    }

    if (m_defaultValue.Any())
    {
        Memory::Copy(destination, m_defaultValue.Data(), m_size);

        return;
    }

    Memory::Zero(destination, m_size);
}

void DynamicStructInstance::CopyConstructInPlace(void* destination, const void* source) const
{
    if (m_functions.copyConstruct != nullptr)
    {
        m_functions.copyConstruct(GetFunctionContext(), destination, source);

        return;
    }

    Memory::Copy(destination, source, m_size);
}

void DynamicStructInstance::MoveConstructInPlace(void* destination, void* source) const
{
    if (m_functions.moveConstruct != nullptr)
    {
        m_functions.moveConstruct(GetFunctionContext(), destination, source);

        return;
    }

    CopyConstructInPlace(destination, source);
}

void DynamicStructInstance::DestructInPlace(void* target) const
{
    if (m_functions.destruct != nullptr)
    {
        m_functions.destruct(GetFunctionContext(), target);
    }
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
