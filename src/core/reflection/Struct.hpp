/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Class.hpp>
#include <core/reflection/BoxedValue.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOM.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedClass;
} // namespace dotnet

namespace serialization {
class FBOMLoadContext;
} // namespace serialization

class Struct : public Class
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

    virtual FBOMResult SerializeStruct(ConstAnyRef value, FBOMObject& out) const = 0;
    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const = 0;

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override = 0;

    HYP_API bool CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const;
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

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_SCOPE;
        HYP_CORE_ASSERT(in.Is<T>());

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(GetTypeId(), /* allowFallback */ (GetSerializationMode() & ClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal)
        {
            if (FBOMResult err = marshal->Serialize(in, out))
            {
                return err;
            }

            return FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & ClassSerializationMode::BITWISE)
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

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        HYP_SCOPE;

        if (!in.GetType().IsType(FBOMObjectType(this)))
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize object into struct - type mismatch" };
        }

        const FBOMMarshalerBase* marshal = (GetSerializationMode() & ClassSerializationMode::USE_MARSHAL_CLASS)
            ? FBOM::GetInstance().GetMarshal(GetTypeId(), /* allowFallback */ (GetSerializationMode() & ClassSerializationMode::MEMBERWISE))
            : nullptr;

        if (marshal)
        {
            if (FBOMResult err = marshal->Deserialize(context, in, out))
            {
                return err;
            }

            return FBOMResult::FBOM_OK;
        }

        if (GetSerializationMode() & ClassSerializationMode::BITWISE)
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

                out = BoxedValue(std::move(result));

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

        out = BoxedValue(std::move(array));

        return true;
    }
};

using DynamicStructInstance_CopyFunction = void* (*)(const void*);
using DynamicStructInstance_DestructFunction = void (*)(void*);

class DynamicStructInstance final : public Struct
{
public:
    DynamicStructInstance(
        TypeId typeId,
        Name name,
        uint32 size,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members,
        DynamicStructInstance_CopyFunction copyFunction,
        DynamicStructInstance_DestructFunction destructFunction);

    virtual ~DynamicStructInstance() override;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override;
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual bool ToBoxed(ByteView memory, BoxedValue& out) const override;

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    DynamicStructInstance_CopyFunction m_copyFunction;
    DynamicStructInstance_DestructFunction m_destructFunction;
};

} // namespace Hyperion
