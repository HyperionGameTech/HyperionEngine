/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_STRUCT_HPP
#define HYPERION_CORE_HYP_STRUCT_HPP

#include <core/object/HypClass.hpp>

namespace hyperion {

namespace dotnet {
class Class;
} // namespace dotnet

struct HypData;

class HypStruct : public HypClass
{
protected:
    // using InitializeCallback = Proc<void, void *, uint32>;

public:
    HypStruct(TypeID type_id, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(type_id, flags, members)
    {
    }

    virtual ~HypStruct() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        return HypClassAllocationMethod::NONE;
    }

    virtual Name GetName() const override = 0;
    virtual SizeType GetSize() const override = 0;

    virtual const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const override
    {
        return nullptr;
    }

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override = 0;

    virtual void ConstructFromBytes(ConstByteView view, Any &out) const = 0;

protected:
    HYP_API bool CreateStructInstance(dotnet::ObjectReference &out_object_reference, const void *object_ptr, SizeType size) const;
};

template <class T>
class HypStructInstance : public HypStruct
{
public:
    static HypStructInstance &GetInstance(EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypStructInstance instance { flags, members };

        return instance;
    }

    HypStructInstance(EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypStruct(TypeID::ForType<T>(), flags, members)
    {
    }

    virtual ~HypStructInstance() override = default;

    virtual Name GetName() const override
    {
        static const Name name = CreateNameFromDynamicString(TypeNameWithoutNamespace<T>().Data());

        return name;
    }

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }

    virtual void ConstructFromBytes(ConstByteView view, Any &out) const override
    {
        AssertThrow(view.Size() == sizeof(T));

        ValueStorage<T> result_storage;
        Memory::MemCpy(result_storage.GetPointer(), view.Data(), sizeof(T));

        out = std::move(result_storage.Get());
    }

    // virtual void Construct(ByteView out_byte_view) const override
    // {
    //     AssertThrow(out_byte_view.Size() == sizeof(T));

    //     new (reinterpret_cast<void *>(out_byte_view.Data())) T();
    // }

    // virtual void CopyConstruct(ConstAnyRef instance_ref, ByteView out_byte_view) const override
    // {
    //     AssertThrow(instance_ref.Is<T>());
    //     AssertThrow(out_byte_view.Size() == sizeof(T));

    //     new (reinterpret_cast<void *>(out_byte_view.Data())) T(instance_ref.Get<T>());
    // }

    // virtual void Destruct(ByteView byte_view) const override
    // {
    //     AssertThrow(byte_view.Size() == sizeof(T));

    //     reinterpret_cast<T *>(byte_view.Data())->~T();
    // }
    
    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        AssertThrow(object_ptr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(out_object_reference, object_ptr, sizeof(T))) {
            return false;
        }

        return true;
    }

    T CreateInstance() const
    {
        ValueStorage<T> result_storage;
        CreateInstance_Internal(result_storage.GetPointer());

        return result_storage.Get();
    }

protected:
    virtual void CreateInstance_Internal(void *out_ptr) const override
        { Memory::Construct<T>(out_ptr); }

    virtual void CreateInstance_Internal(Any &out) const override
        { out.Emplace<T>(); }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
        { return HashCode::GetHashCode(ref.Get<T>()); }
};

} // namespace hyperion

#endif