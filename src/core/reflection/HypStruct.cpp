/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypStruct.hpp>
#include <core/reflection/HypClassRegistry.hpp>

#include <core/reflection/TypeInfo.hpp>

#ifdef HYP_DOTNET
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#endif

namespace hyperion {

#pragma region HypStruct

bool HypStruct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        SizeType size;
    };

    HYP_CORE_ASSERT(objectPtr != nullptr);

#ifdef HYP_DOTNET
    if (dotnet::Class* managedClass = HypClass::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        outObjectReference = managedClass->NewManagedObject(&context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                HYP_CORE_ASSERT(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %zu but got %u",
                    context.size, objectSize);

                Memory::MemCpy(objectPtr, context.ptr, context.size);
            });

        return true;
    }
#endif

    return false;
}

#pragma endregion HypStruct

#pragma region DynamicHypStructInstance

DynamicHypStructInstance::DynamicHypStructInstance(
    TypeId typeId,
    Name name,
    uint32 size,
    Span<const HypClassAttribute> attributes,
    EnumFlags<HypClassFlags> flags,
    Span<HypMember> members,
    DynamicHypStructInstance_CopyFunction copyFunction,
    DynamicHypStructInstance_DestructFunction destructFunction)
    : HypStruct(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members),
      m_copyFunction(copyFunction),
      m_destructFunction(destructFunction)
{
    Assert(m_copyFunction != nullptr);
    Assert(m_destructFunction != nullptr);
    Assert(size > 0);

    m_size = size;
    m_alignment = alignof(void*);

    // @TODO Register the ManagedClass (dotnet::Class) for this. We need the assembly.
    HypClassRegistry::GetInstance().RegisterClass(typeId, this);
}

DynamicHypStructInstance::~DynamicHypStructInstance()
{
    HypClassRegistry::GetInstance().UnregisterClass(this);
}

#ifdef HYP_DOTNET
bool DynamicHypStructInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
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

bool DynamicHypStructInstance::ToHypData(ByteView memory, HypData& out) const
{
    void* data = Memory::Allocate(m_size);
    Memory::MemCpy(data, memory.Data(), m_size);

    const TypeInfo& typeInfo = TypeInfo::ForHypClass(this);

    out = HypData(Any::FromVoidPointer(&typeInfo, data, m_copyFunction, m_destructFunction));

    return true;
}

#pragma endregion DynamicHypStructInstance

} // namespace hyperion