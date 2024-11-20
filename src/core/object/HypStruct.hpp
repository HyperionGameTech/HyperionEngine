/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_STRUCT_HPP
#define HYPERION_CORE_HYP_STRUCT_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

namespace hyperion {

namespace dotnet {
class Class;
} // namespace dotnet

class HypStruct : public HypClass
{
public:
    HypStruct(TypeID type_id, Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(type_id, name, parent_name, attributes, flags, members)
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

    virtual SizeType GetSize() const override = 0;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;
    
    virtual void ConstructFromBytes(ConstByteView view, HypData &out) const = 0;

    virtual fbom::FBOMResult SerializeStruct(ConstAnyRef value, fbom::FBOMObject &out) const = 0;
    virtual fbom::FBOMResult DeserializeStruct(const fbom::FBOMObject &in, HypData &out) const = 0;

protected:
    virtual IHypObjectInitializer *GetObjectInitializer_Internal(void *object_ptr) const override
    {
        return nullptr;
    }

    virtual void CreateInstance_Internal(HypData &out) const override = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override = 0;

    HYP_API bool CreateStructInstance(dotnet::ObjectReference &out_object_reference, const void *object_ptr, SizeType size) const;
};

template <class T>
class HypStructInstance final : public HypStruct
{
public:
    static HypStructInstance &GetInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypStructInstance instance { name, parent_name, attributes, flags, members };

        return instance;
    }

    HypStructInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypStruct(TypeID::ForType<T>(), name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypStructInstance() override = default;

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }
    
    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        AssertThrow(object_ptr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(out_object_reference, object_ptr, sizeof(T))) {
            return false;
        }

        return true;
    }

    virtual bool CanCreateInstance() const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            return true;
        } else {
            return false;
        }
    }

    virtual void ConstructFromBytes(ConstByteView view, HypData &out) const override
    {
        AssertThrow(view.Size() == sizeof(T));

        ValueStorage<T> result_storage;
        Memory::MemCpy(result_storage.GetPointer(), view.Data(), sizeof(T));

        out = HypData(std::move(result_storage.Get()));
    }

    virtual fbom::FBOMResult SerializeStruct(ConstAnyRef in, fbom::FBOMObject &out) const override
    {
        HYP_SCOPE;
        AssertThrow(in.Is<T>());
        
        const fbom::FBOMMarshalerBase *marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>(), /* allow_fallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal) {
            if (fbom::FBOMResult err = marshal->Serialize(in, out)) {
                return err;
            }

            return fbom::FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & HypClassSerializationMode::BITWISE) {
            fbom::FBOMData struct_data = fbom::FBOMData::FromStructUnchecked(in.Get<T>());
            
            fbom::FBOMObject struct_wrapper_object(fbom::FBOMObjectType(this));
            struct_wrapper_object.SetProperty("StructData", std::move(struct_data));

            out = std::move(struct_wrapper_object);

            return { fbom::FBOMResult::FBOM_OK };
        }

        return { fbom::FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };

        // return HypDataHelper<T>::Serialize(HypData(AnyRef(value.GetTypeID(), const_cast<void *>(value.GetPointer()))), out);
    }

    virtual fbom::FBOMResult DeserializeStruct(const fbom::FBOMObject &in, HypData &out) const override
    {
        HYP_SCOPE;

        if (!in.GetType().Is(fbom::FBOMObjectType(this))) {
            return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize object into struct - type mismatch" };
        }
        
        const fbom::FBOMMarshalerBase *marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>(), /* allow_fallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal) {
            if (fbom::FBOMResult err = marshal->Deserialize(in, out)) {
                return err;
            }

            return fbom::FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & HypClassSerializationMode::BITWISE) {
            // Read StructData property
            T result;
            
            if (fbom::FBOMResult err = in.GetProperty("StructData").ReadStruct<T, /* CompileTimeChecked */ false>(&result)) {
                return err;
            }

            out = HypData(std::move(result));

            return { fbom::FBOMResult::FBOM_OK };
        }

        return { fbom::FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            out = T { };
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    virtual void CreateInstance_Internal(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HasGetHashCode<T, HashCode>::value) {
            return HashCode::GetHashCode(ref.Get<T>());
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace hyperion

#endif