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

namespace serialization {
class FBOMLoadContext;
} // namespace serialization

class HypStruct : public HypClass
{
public:
    HypStruct(TypeID type_id, Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(type_id, name, static_index, num_descendants, parent_name, attributes, flags, members)
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

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;

    virtual bool ToHypData(ByteView memory, HypData& out_hyp_data) const override = 0;

    virtual FBOMResult SerializeStruct(ConstAnyRef value, FBOMObject& out) const = 0;
    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const = 0;

protected:
    virtual void FixupPointer(void* target, IHypObjectInitializer* new_initializer) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void PostLoad_Internal(void* object_ptr) const override
    {
    }

    virtual IHypObjectInitializer* GetObjectInitializer_Internal(void* object_ptr) const override
    {
        return nullptr;
    }

    virtual bool CreateInstance_Internal(HypData& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override = 0;

    HYP_API bool CreateStructInstance(dotnet::ObjectReference& out_object_reference, const void* object_ptr, SizeType size) const;
};

template <class T>
class HypStructInstance final : public HypStruct
{
public:
    using PostLoadCallback = void (*)(T&);

    static HypStructInstance& GetInstance(
        Name name,
        int static_index,
        uint32 num_descendants,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
    {
        static HypStructInstance instance { name, static_index, num_descendants, parent_name, attributes, flags, members };

        return instance;
    }

    HypStructInstance(
        Name name,
        int static_index,
        uint32 num_descendants,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
        : HypStruct(TypeID::ForType<T>(), name, static_index, num_descendants, parent_name, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypStructInstance() override = default;

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override
    {
        AssertThrow(object_ptr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(out_object_reference, object_ptr, sizeof(T)))
        {
            return false;
        }

        return true;
    }

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

    virtual bool ToHypData(ByteView memory, HypData& out_hyp_data) const override
    {
        if constexpr (std::is_abstract_v<T>)
        {
            return false;
        }
        else
        {
            AssertThrow(memory.Size() == sizeof(T));

            out_hyp_data = HypData(std::move(*reinterpret_cast<T*>(memory.Data())));

            return true;
        }
    }

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_SCOPE;
        AssertThrow(in.Is<T>());

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(TypeID::ForType<T>(), /* allow_fallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal)
        {
            if (FBOMResult err = marshal->Serialize(in, out))
            {
                return err;
            }

            return FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & HypClassSerializationMode::BITWISE)
        {
            if constexpr (std::is_abstract_v<T>)
            {
                return { FBOMResult::FBOM_ERR, "Cannot use bitwise serialization with abstract type!" };
            }
            else
            {
                FBOMData struct_data = FBOMData::FromStructUnchecked(in.Get<T>());

                FBOMObject struct_wrapper_object(FBOMObjectType(this));
                struct_wrapper_object.SetProperty("StructData", std::move(struct_data));

                out = std::move(struct_wrapper_object);

                return { FBOMResult::FBOM_OK };
            }
        }

        return { FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        HYP_SCOPE;

        if (!in.GetType().Is(FBOMObjectType(this)))
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize object into struct - type mismatch" };
        }

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(TypeID::ForType<T>(), /* allow_fallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal)
        {
            if (FBOMResult err = marshal->Deserialize(context, in, out))
            {
                return err;
            }

            return FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & HypClassSerializationMode::BITWISE)
        {
            if constexpr (std::is_abstract_v<T>)
            {
                return { FBOMResult::FBOM_ERR, "Cannot use bitwise serialization with abstract type!" };
            }
            else
            {
                // Read StructData property
                T result {};

                if (FBOMResult err = in.GetProperty("StructData").ReadStruct<T, /* CompileTimeChecked */ false>(&result))
                {
                    return err;
                }

                out = HypData(std::move(result));

                return { FBOMResult::FBOM_OK };
            }
        }

        return { FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

protected:
    virtual void PostLoad_Internal(void* object_ptr) const override
    {
        if (!object_ptr)
        {
            return;
        }

        const IHypClassCallbackWrapper* callback_wrapper = HypClassCallbackCollection<HypClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeID());

        if (!callback_wrapper)
        {
            return;
        }

        const HypClassCallbackWrapper<PostLoadCallback>* callback_wrapper_casted = dynamic_cast<const HypClassCallbackWrapper<PostLoadCallback>*>(callback_wrapper);
        AssertThrow(callback_wrapper_casted != nullptr);

        callback_wrapper_casted->GetCallback()(*static_cast<T*>(object_ptr));
    }

    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            out = HypData(T {});

            return true;
        }
        else
        {
            return false;
        }
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        Array<T> array;
        array.Reserve(elements.Size());

        for (SizeType i = 0; i < elements.Size(); i++)
        {
            if (!elements[i].Is<T>())
            {
                return false;
            }

            array.PushBack(std::move(elements[i].Get<T>()));
        }

        out = HypData(std::move(array));

        // debugging
        AssertDebug(out.Is<Array<T>>());
        AssertThrow(out.GetTypeID() == TypeID::ForType<Array<T>>());
        DebugLog(LogType::Debug, "Created HypData for array type: %s from array with size %zu, TypeID: %u",
            TypeNameWithoutNamespace<decltype(array)>().Data(),
            out.Get<Array<T>>().Size(),
            TypeID::ForType<decltype(array)>().Value());

        return true;
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HYP_HAS_METHOD(T, GetHashCode))
        {
            return HashCode::GetHashCode(ref.Get<T>());
        }
        else
        {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace hyperion

#endif