/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/Struct.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/reflection/TypeInfo.hpp>

#ifdef HYP_DOTNET
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#endif

namespace hyperion {

#pragma region Struct

bool Struct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        SizeType size;
    };

    AssertDebug(objectPtr != nullptr);

#ifdef HYP_DOTNET
    if (dotnet::ManagedClass* managedClass = Class::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        outObjectReference = managedClass->NewManagedObject(&context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                AssertDebug(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %zu but got %u",
                    context.size, objectSize);

                Memory::MemCpy(objectPtr, context.ptr, context.size);
            });

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
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<HypMember> members,
    DynamicStructInstance_CopyFunction copyFunction,
    DynamicStructInstance_DestructFunction destructFunction)
    : Struct(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members),
      m_copyFunction(copyFunction),
      m_destructFunction(destructFunction)
{
    Assert(m_copyFunction != nullptr);
    Assert(m_destructFunction != nullptr);
    Assert(size > 0);

    m_size = size;
    m_alignment = alignof(void*);

    /// \todo Register the ManagedClass (dotnet::ManagedClass) for this. We need the assembly.
    ClassRegistry::GetInstance().RegisterClass(typeId, this);
}

DynamicStructInstance::~DynamicStructInstance()
{
    ClassRegistry::GetInstance().UnregisterClass(this);
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

bool DynamicStructInstance::ToHypData(ByteView memory, BoxedValue& out) const
{
    void* data = Memory::Allocate(m_size);
    Memory::MemCpy(data, memory.Data(), m_size);

    const TypeInfo* pTypeInfo = GetTypeInfo();
    AssertDebug(pTypeInfo != nullptr);

    out = BoxedValue(Any::FromVoidPointer(pTypeInfo, data, m_copyFunction, m_destructFunction));

    return true;
}

#pragma endregion DynamicStructInstance

} // namespace hyperion