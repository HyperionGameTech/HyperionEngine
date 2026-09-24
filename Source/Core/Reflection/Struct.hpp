/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/BoxedValue.hpp>

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

    virtual bool CanConstructInPlace() const = 0;
    virtual bool CanCopyConstructInPlace() const = 0;
    virtual bool CanMoveConstructInPlace() const = 0;

    virtual void ConstructInPlace(void* destination) const = 0;
    virtual void CopyConstructInPlace(void* destination, const void* source) const = 0;
    virtual void MoveConstructInPlace(void* destination, void* source) const = 0;
    virtual void DestructInPlace(void* target) const = 0;

    virtual bool IsTriviallyCopyable() const = 0;

    bool ConstructBoxed(BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo = nullptr) const;
    bool CopyConstructBoxed(const void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo = nullptr) const;
    bool MoveConstructBoxed(void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo = nullptr) const;

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override = 0;

    bool CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, size_t size) const;

private:
    void* AllocateBoxedObject() const;
    void MakeBoxFromObject(void* object, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const;
};

HYP_FORCE_INLINE const Struct* GetStructFromClass(const Class* cls)
{
    if (!cls || !cls->IsStructType())
    {
        return nullptr;
    }

    return static_cast<const Struct*>(cls);
}

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

    virtual bool CanConstructInPlace() const override
    {
        return std::is_default_constructible_v<T> && !std::is_abstract_v<T>;
    }

    virtual bool CanCopyConstructInPlace() const override
    {
        return std::is_copy_constructible_v<T> && !std::is_abstract_v<T>;
    }

    virtual bool CanMoveConstructInPlace() const override
    {
        return (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>) && !std::is_abstract_v<T>;
    }

    virtual void ConstructInPlace(void* destination) const override
    {
        if constexpr (std::is_default_constructible_v<T> && !std::is_abstract_v<T>)
        {
            new (destination) T();
        }
        else
        {
            HYP_CORE_ASSERT(false, "Struct type cannot be default constructed in place");
        }
    }

    virtual void CopyConstructInPlace(void* destination, const void* source) const override
    {
        if constexpr (std::is_copy_constructible_v<T> && !std::is_abstract_v<T>)
        {
            new (destination) T(*static_cast<const T*>(source));
        }
        else
        {
            HYP_CORE_ASSERT(false, "Struct type cannot be copy constructed in place");
        }
    }

    virtual void MoveConstructInPlace(void* destination, void* source) const override
    {
        if constexpr (std::is_move_constructible_v<T> && !std::is_abstract_v<T>)
        {
            new (destination) T(std::move(*static_cast<T*>(source)));
        }
        else if constexpr (std::is_copy_constructible_v<T> && !std::is_abstract_v<T>)
        {
            new (destination) T(*static_cast<const T*>(source));
        }
        else
        {
            HYP_CORE_ASSERT(false, "Struct type cannot be move constructed in place");
        }
    }

    virtual void DestructInPlace(void* target) const override
    {
        if constexpr (!std::is_trivially_destructible_v<T> && !std::is_abstract_v<T>)
        {
            static_cast<T*>(target)->~T();
        }
    }

    virtual bool IsTriviallyCopyable() const override
    {
        return std::is_trivially_copyable_v<T>;
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

        const ClassCallbackWrapper<PostLoadCallback>* callbackWrapperCasted = static_cast<const ClassCallbackWrapper<PostLoadCallback>*>(callbackWrapper);
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
    void (*destruct)(void* ctx, void* ptr);
    void (*copyConstruct)(void* ctx, void* dest, const void* src) = nullptr;
    void (*moveConstruct)(void* ctx, void* dest, void* src) = nullptr;
    void* context = nullptr;
};

struct DynamicStructFieldDesc
{
    Name name;
    uint32 offset = 0;
    uint32 size = 0;                    // every element of an array included
    const TypeInfo* typeInfo = nullptr; // the element type for arrays
    uint32 arrayLength = 0;             // > 0: a fixed-size inline array, reflected as an Array<BoxedValue> that can't be resized
    bool isTransient = false;           // kept across layout changes, but never serialized or shown in the editor
};

CORE_API bool MakeDynamicStructProperty(const DynamicStructFieldDesc& fieldDesc, int editorOrder, MemberVariant& outMember);

struct DynamicStructDesc
{
    TypeId typeId;
    Name name;
    uint32 size = 0;
    uint32 alignment = alignof(void*);
    const void* defaultValue = nullptr; // `size` bytes; nullptr = zero-filled
    Span<const DynamicStructFieldDesc> fields;
};

CORE_API DynamicStructInstance* CreateDynamicStruct(const DynamicStructDesc& desc);
CORE_API const Struct* GetBoxingStruct(const BoxedValue& value);
CORE_API bool AssignStructValue(const Struct& targetStruct, void* destination, const BoxedValue& value);
CORE_API void CopyMatchingProperties(const Struct& sourceStruct, const BoxedValue& source, const Struct& targetStruct, BoxedValue& target);

class CORE_API DynamicStructInstance final : public Struct
{
public:
    DynamicStructInstance(
        TypeId typeId,
        Name name,
        uint32 size,
        uint32 alignment,
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

    /*! \brief Sets template for default construction copies when no construct function is given, from raw bytes  */
    void SetDefaultValue(const void* defaultValue);

    void AddRef();
    void Release();

    virtual bool ToBoxed(ByteView memory, BoxedValue& out) const override;

    virtual bool CanConstructInPlace() const override
    {
        return true;
    }

    virtual bool CanCopyConstructInPlace() const override
    {
        return true;
    }

    virtual bool CanMoveConstructInPlace() const override
    {
        return true;
    }

    virtual void ConstructInPlace(void* destination) const override;
    virtual void CopyConstructInPlace(void* destination, const void* source) const override;
    virtual void MoveConstructInPlace(void* destination, void* source) const override;
    virtual void DestructInPlace(void* target) const override;

    virtual bool IsTriviallyCopyable() const override
    {
        return !m_functions.copyConstruct && !m_functions.moveConstruct && !m_functions.destruct;
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override;

    HYP_FORCE_INLINE void* GetFunctionContext() const
    {
        return m_functions.context != nullptr ? m_functions.context : const_cast<void*>(static_cast<const void*>(this));
    }

    DynamicStructInstanceFunctions m_functions;
    ByteBuffer m_defaultValue;

    volatile int32 m_refCount;
};

} // namespace Hyperion
