/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Class.hpp>
#include <Core/reflection/BoxedValue.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedClass;
} // namespace dotnet

class CORE_API Struct : public Class
{
public:
    Struct(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<MemberVariant> members)
        : Class(typeId, name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
    }

    virtual ~Struct() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual ClassAllocationMethod GetAllocationMethod() const override
    {
        return ClassAllocationMethod::NONE;
    }

    virtual bool CanCreateInstance() const override = 0;

    virtual bool ToBoxed(ByteView memory, BoxedValue& outBoxed) const override = 0;

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override = 0;

    bool CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, size_t size) const;
};

template <class T>
class StructInstance final : public Struct
{
public:
    static_assert(!std::is_base_of_v<ObjectBase, T>, "Type derives from ObjectBase; use HYP_CLASS instead.");

    using PostLoadCallback = void (*)(T&);

    static StructInstance& GetInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members)
    {
        static StructInstance s_instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return s_instance;
    }

    StructInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members)
        : Struct(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~StructInstance() override = default;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override
    {
        HYP_CORE_ASSERT(objectPtr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(outObjectReference, objectPtr, sizeof(T)))
        {
            return false;
        }

        return true;
    }
#endif

    virtual bool CanCreateInstance() const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    virtual bool ToBoxed(ByteView memory, BoxedValue& outBoxed) const override
    {
        if constexpr (std::is_abstract_v<T>)
        {
            return false;
        }
        else
        {
            HYP_CORE_ASSERT(memory.Size() == sizeof(T));

            outBoxed = BoxedValue(std::move(*reinterpret_cast<T*>(memory.Data())));

            return true;
        }
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
        if (!objectPtr)
        {
            return;
        }

        const IClassCallbackWrapper* callbackWrapper = ClassCallbackCollection<ClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeId());

        if (!callbackWrapper)
        {
            return;
        }

        const ClassCallbackWrapper<PostLoadCallback>* callbackWrapperCasted = dynamic_cast<const ClassCallbackWrapper<PostLoadCallback>*>(callbackWrapper);
        HYP_CORE_ASSERT(callbackWrapperCasted != nullptr);

        callbackWrapperCasted->GetCallback()(*static_cast<T*>(objectPtr));
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            out = BoxedValue(T {});

            return true;
        }
        else
        {
            return false;
        }
    }

    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override
    {
        if constexpr (std::is_copy_constructible_v<T>)
        {
            // ok, we need copy constructible for GenericArrayWrapper
            Array<T> array;
            array.Reserve(elements.Size());

            for (size_t i = 0; i < elements.Size(); i++)
            {
                if (!elements[i].Is<T>())
                {
                    return false;
                }

                array.PushBack(std::move(elements[i].Get<T>()));
            }

            out = BoxedValue(std::move(array));

            return true;
        }

        return false;
    }
};

class DynamicStructInstance;

struct DynamicStructInstanceFunctions
{
    void (*construct)(void* ctx, void* dest);
    void* (*copy)(void* ctx, const void* src);
    void (*destruct)(void* ctx, void* ptr);
};

class CORE_API DynamicStructInstance final : public Struct
{
public:
#ifdef HYP_SCRIPT
    DynamicStructInstance(
        TypeId typeId,
        Name name,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members,
        const DynamicStructInstanceFunctions& functions);
#endif

    DynamicStructInstance(
        TypeId typeId,
        Name name,
        uint32 size,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members,
        const DynamicStructInstanceFunctions& functions);

    virtual ~DynamicStructInstance() override;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override;
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    HYP_FORCE_INLINE const DynamicStructInstanceFunctions& GetFunctions() const
    {
        return m_functions;
    }

    void AddRef();
    void Release();

    virtual bool ToBoxed(ByteView memory, BoxedValue& out) const override;

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override;

    DynamicStructInstanceFunctions m_functions;

    volatile int32 m_refCount;
};

} // namespace Hyperion
