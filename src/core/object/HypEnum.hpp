/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/TypeAttributes.hpp>

namespace hyperion {

template <class T>
struct HypEnumRegistration;

class HypEnum : protected HypClass
{
public:
    HypEnum(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(typeId, name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
    }

    virtual ~HypEnum() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        return HypClassAllocationMethod::NONE;
    }

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override = 0;
#endif

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeId GetUnderlyingTypeId() const override = 0;

protected:
    virtual bool CreateInstance_Internal(HypData& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override = 0;
};

template <class T>
class HypEnumInstance final : protected HypEnum
{
public:
    template <class U>
    friend struct HypEnumRegistration;

    static HypEnumInstance& GetInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypEnumInstance instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return instance;
    }

    HypEnumInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypEnum(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypEnumInstance() override = default;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override
    {
        HYP_NOT_IMPLEMENTED();
    }
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual TypeId GetUnderlyingTypeId() const override
    {
        static const TypeId typeId = TypeId::ForType<std::underlying_type_t<T>>();

        return typeId;
    }

protected:
    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        out = HypData(T {});

        return true;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        Array<T> array;
        array.ResizeUninitialized(elements.Size());

        for (SizeType i = 0; i < elements.Size(); i++)
        {
            // strict = false to allow any numeric type.
            if (!elements[i].Is<std::underlying_type_t<T>>(/* strict */ false))
            {
                return false;
            }

            array[i] = elements[i].Get<T>();
        }

        out = HypData(std::move(array));

        return true;
    }
};

} // namespace hyperion
