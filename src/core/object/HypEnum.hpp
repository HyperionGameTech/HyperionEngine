/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_ENUM_HPP
#define HYPERION_CORE_HYP_ENUM_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/containers/StaticArray.hpp>

#include <core/utilities/TypeAttributes.hpp>

namespace hyperion {

namespace detail {

template <class TEnum, SizeType... Indices>
constexpr auto MakeEnumStaticArray(std::index_sequence<Indices...>)
{
    return StaticArray<TEnum, sizeof...(Indices)> { TEnum(Indices)... };
}

} // namespace detail

class HypEnum : public HypClass
{
public:
    HypEnum(TypeID type_id, Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
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

    virtual SizeType GetSize() const override = 0;
    virtual SizeType GetAlignment() const override = 0;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeID GetUnderlyingTypeID() const = 0;

protected:
    virtual void FixupPointer(void *target, IHypObjectInitializer *new_initializer) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual IHypObjectInitializer *GetObjectInitializer_Internal(void *object_ptr) const override
    {
        return nullptr;
    }

    virtual void CreateInstance_Internal(HypData &out) const override = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override = 0;
};

template <class T>
class HypEnumInstance final : public HypEnum
{
public:
    static HypEnumInstance &GetInstance(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypEnumInstance instance { name, static_index, num_descendants, parent_name, attributes, flags, members };

        return instance;
    }

    HypEnumInstance(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypEnum(TypeID::ForType<T>(), name, static_index, num_descendants, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypEnumInstance() override = default;

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
        HYP_NOT_IMPLEMENTED();
    }

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual TypeID GetUnderlyingTypeID() const override
    {
        static const TypeID type_id = TypeID::ForType<std::underlying_type_t<T>>();

        return type_id;
    }

    /*! \brief Returns a compile-time StaticArray of all the enum values. */
    static constexpr auto GetValues_Static()
    {
        return detail::MakeEnumStaticArray<T>(std::make_index_sequence<SizeType(T::MAX)>());
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        out = HypData(T { });
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        return HashCode::GetHashCode(ref.Get<T>());
    }
};

} // namespace hyperion

#endif