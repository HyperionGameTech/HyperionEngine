/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
    HypStruct(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(typeId, name, staticIndex, numDescendants, parentName, attributes, flags, members)
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

    virtual bool CanCreateInstance() const override = 0;

    virtual bool ToHypData(ByteView memory, HypData& outHypData) const override = 0;

    virtual FBOMResult SerializeStruct(ConstAnyRef value, FBOMObject& out) const = 0;
    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const = 0;

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(HypData& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override = 0;

    HYP_API bool CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const;
};

template <class T>
class HypStructInstance final : public HypStruct
{
public:
    static_assert(!std::is_base_of_v<HypObjectBase, T>, "Type derives from HypObjectBase; use HYP_CLASS instead.");

    using PostLoadCallback = void (*)(T&);

    static HypStructInstance& GetInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
    {
        static HypStructInstance instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return instance;
    }

    HypStructInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
        : HypStruct(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypStructInstance() override = default;

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

    virtual bool ToHypData(ByteView memory, HypData& outHypData) const override
    {
        if constexpr (std::is_abstract_v<T>)
        {
            return false;
        }
        else
        {
            HYP_CORE_ASSERT(memory.Size() == sizeof(T));

            outHypData = HypData(std::move(*reinterpret_cast<T*>(memory.Data())));

            return true;
        }
    }

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_SCOPE;
        HYP_CORE_ASSERT(in.Is<T>());

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(GetTypeId(), /* allowFallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
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
                FBOMData structData = FBOMData::FromStructUnchecked(in.Get<T>());

                FBOMObject structWrapperObject(FBOMObjectType(this));
                structWrapperObject.SetProperty("StructData", std::move(structData));

                out = std::move(structWrapperObject);

                return { FBOMResult::FBOM_OK };
            }
        }

        return { FBOMResult::FBOM_ERR, "Type does not have an associated marshal class registered, and is not marked as bitwise serializable" };
    }

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        HYP_SCOPE;

        if (!in.GetType().IsType(FBOMObjectType(this)))
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize object into struct - type mismatch" };
        }

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & HypClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(GetTypeId(), /* allowFallback */ (GetSerializationMode() & HypClassSerializationMode::MEMBERWISE))
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
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
        if (!objectPtr)
        {
            return;
        }

        const IHypClassCallbackWrapper* callbackWrapper = HypClassCallbackCollection<HypClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeId());

        if (!callbackWrapper)
        {
            return;
        }

        const HypClassCallbackWrapper<PostLoadCallback>* callbackWrapperCasted = dynamic_cast<const HypClassCallbackWrapper<PostLoadCallback>*>(callbackWrapper);
        HYP_CORE_ASSERT(callbackWrapperCasted != nullptr);

        callbackWrapperCasted->GetCallback()(*static_cast<T*>(objectPtr));
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
        HYP_CORE_ASSERT(out.Is<Array<T>>());
        HYP_CORE_ASSERT(out.GetTypeId() == TypeId::ForType<Array<T>>());

        return true;
    }
};

using DynamicHypStructInstance_CopyFunction = void* (*)(const void*);
using DynamicHypStructInstance_DestructFunction = void (*)(void*);

class DynamicHypStructInstance final : public HypStruct
{
public:
    DynamicHypStructInstance(
        TypeId typeId,
        Name name,
        uint32 size,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members,
        DynamicHypStructInstance_CopyFunction copyFunction,
        DynamicHypStructInstance_DestructFunction destructFunction);

    virtual ~DynamicHypStructInstance() override;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override;
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual bool ToHypData(ByteView memory, HypData& out) const override;

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    DynamicHypStructInstance_CopyFunction m_copyFunction;
    DynamicHypStructInstance_DestructFunction m_destructFunction;
};

} // namespace hyperion
