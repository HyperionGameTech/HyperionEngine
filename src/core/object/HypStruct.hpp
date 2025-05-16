/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_STRUCT_HPP
#define HYPERION_CORE_HYP_STRUCT_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOM.hpp>

namespace hyperion {

namespace dotnet {
class Class;
} // namespace dotnet

namespace fbom {
class FBOMLoadContext;
} // namespace fbom

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
    virtual SizeType GetAlignment() const override = 0;

    virtual SizeType GetObjectInitializerOffset() const override final
    {
        return 0;
    }

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;

    virtual bool ToHypData(ByteView memory, HypData &out_hyp_data) const override = 0;

    virtual void PostLoad(void *object_ptr) const override { }

    virtual fbom::FBOMResult SerializeStruct(ConstAnyRef value, fbom::FBOMObject &out) const = 0;
    virtual fbom::FBOMResult DeserializeStruct(fbom::FBOMLoadContext &context, const fbom::FBOMObject &in, HypData &out) const = 0;

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
    using PostLoadCallback = void (*)(T &);

    static HypStructInstance &GetInstance(
        Name name,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members
    )
    {
        static HypStructInstance instance { name, parent_name, attributes, flags, members };

        return instance;
    }

    HypStructInstance(
        Name name,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members
    ) : HypStruct(TypeID::ForType<T>(), name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypStructInstance() override = default;

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }

    virtual SizeType GetAlignment() const override
    {
        return alignof(T);
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

    virtual bool ToHypData(ByteView memory, HypData &out_hyp_data) const override
    {
        if constexpr (std::is_abstract_v<T>) {
            return false;
        } else {
            AssertThrow(memory.Size() == sizeof(T));

            out_hyp_data = HypData(std::move(*reinterpret_cast<T *>(memory.Data())));

            return true;
        }
    }

    virtual void PostLoad(void *object_ptr) const override
    {
        const IHypClassCallbackWrapper *callback_wrapper = HypClassCallbackCollection<HypClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeID());

        if (!callback_wrapper) {
            return;
        }

        const HypClassCallbackWrapper<PostLoadCallback> *callback_wrapper_casted = dynamic_cast<const HypClassCallbackWrapper<PostLoadCallback> *>(callback_wrapper);
        AssertThrow(callback_wrapper_casted != nullptr);

        callback_wrapper_casted->GetCallback()(*static_cast<T *>(object_ptr));
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
            if constexpr (std::is_abstract_v<T>) {
                return { fbom::FBOMResult::FBOM_ERR, "Cannot use bitwise serialization with abstract type!" };
            } else {
                fbom::FBOMData struct_data = fbom::FBOMData::FromStructUnchecked(in.Get<T>());
            
                fbom::FBOMObject struct_wrapper_object(fbom::FBOMObjectType(this));
                struct_wrapper_object.SetProperty("StructData", std::move(struct_data));

                out = std::move(struct_wrapper_object);

                return { fbom::FBOMResult::FBOM_OK };
            }
        }

        return { fbom::FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

    virtual fbom::FBOMResult DeserializeStruct(fbom::FBOMLoadContext &context, const fbom::FBOMObject &in, HypData &out) const override
    {
        HYP_SCOPE;

        if (!in.GetType().Is(fbom::FBOMObjectType(this))) {
            return { fbom::FBOMResult::FBOM_ERR, "Cannot deserialize object into struct - type mismatch" };
        }
        
        const fbom::FBOMMarshalerBase *marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? fbom::FBOM::GetInstance().GetMarshal(TypeID::ForType<T>(), /* allow_fallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal) {
            if (fbom::FBOMResult err = marshal->Deserialize(context, in, out)) {
                return err;
            }

            return fbom::FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & HypClassSerializationMode::BITWISE) {
            if constexpr (std::is_abstract_v<T>) {
                return { fbom::FBOMResult::FBOM_ERR, "Cannot use bitwise serialization with abstract type!" };
            } else {
                // Read StructData property
                T result { };
            
                if (fbom::FBOMResult err = in.GetProperty("StructData").ReadStruct<T, /* CompileTimeChecked */ false>(&result)) {
                    return err;
                }

                out = HypData(std::move(result));

                return { fbom::FBOMResult::FBOM_OK };
            }
        }

        return { fbom::FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            out = HypData(T { });
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HYP_HAS_METHOD(T, GetHashCode)) {
            return HashCode::GetHashCode(ref.Get<T>());
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace hyperion

#endif