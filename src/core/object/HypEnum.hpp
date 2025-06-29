/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_ENUM_HPP
#define HYPERION_CORE_HYP_ENUM_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/TypeAttributes.hpp>

namespace hyperion {

class HypEnum : public HypClass
{
public:
    HypEnum(TypeId type_id, Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(type_id, name, static_index, num_descendants, parent_name, attributes, flags, members)
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

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeId GetUnderlyingTypeId() const = 0;

protected:
    virtual void FixupPointer(void* target, IHypObjectInitializer* new_initializer) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual IHypObjectInitializer* GetObjectInitializer_Internal(void* object_ptr) const override
    {
        return nullptr;
    }

    virtual bool CreateInstance_Internal(HypData& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override = 0;
};

template <class T>
class HypEnumInstance final : public HypEnum
{
public:
    static HypEnumInstance& GetInstance(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypEnumInstance instance { name, static_index, num_descendants, parent_name, attributes, flags, members };

        return instance;
    }

    HypEnumInstance(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypEnum(TypeId::ForType<T>(), name, static_index, num_descendants, parent_name, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypEnumInstance() override = default;

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual TypeId GetUnderlyingTypeId() const override
    {
        static const TypeId type_id = TypeId::ForType<std::underlying_type_t<T>>();

        return type_id;
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

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        return HashCode::GetHashCode(ref.Get<T>());
    }
};

} // namespace hyperion

#endif