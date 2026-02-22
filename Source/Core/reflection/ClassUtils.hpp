/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Class.hpp>
#include <Core/reflection/Struct.hpp>
#include <Core/reflection/Enum.hpp>
#include <Core/reflection/MemberVariant.hpp>
#include <Core/reflection/TypeInfo.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/FormatFwd.hpp>

#include <Core/Constants.hpp>

#include <type_traits>

namespace Hyperion {

class ClassRegistrationBase
{
protected:
    ClassRegistrationBase(TypeId typeId, Class* cls);

    const Class* m_class;
};

template <class T>
class ClassRegistration final : public ClassRegistrationBase
{
public:
    static constexpr EnumFlags<ClassFlags> flags = ClassFlags::CLASS_TYPE
        | (IsPodTypeV<T> ? ClassFlags::POD_TYPE : ClassFlags::NONE)
        | (std::is_abstract_v<T> ? ClassFlags::ABSTRACT : ClassFlags::NONE);

    ClassRegistration(const Class** pGlobal, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, Span<MemberVariant> members)
        : ClassRegistrationBase(TypeId::ForType<T>(), &ClassInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<MemberVariant>(members.Begin(), members.End())))
    {
#ifndef HYP_TOOL
        *pGlobal = m_class;
#endif
    }
};

template <class T>
class StructRegistration final : public ClassRegistrationBase
{
public:
    static constexpr EnumFlags<ClassFlags> flags = ClassFlags::STRUCT_TYPE
        | (IsPodTypeV<T> ? ClassFlags::POD_TYPE : ClassFlags::NONE)
        | (std::is_abstract_v<T> ? ClassFlags::ABSTRACT : ClassFlags::NONE);

    StructRegistration(const Class** pGlobal, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, Span<MemberVariant> members)
        : ClassRegistrationBase(TypeId::ForType<T>(), &StructInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<MemberVariant>(members.Begin(), members.End())))
    {
#ifndef HYP_TOOL
        *pGlobal = m_class;
#endif
    }
};

template <class T>
class EnumRegistration final : public ClassRegistrationBase
{
public:
    static constexpr EnumFlags<ClassFlags> flags = ClassFlags::ENUM_TYPE;

    EnumRegistration(const Class** pGlobal, Name name, int staticIndex, uint32 numDescendants, Span<const ClassAttribute> attributes, Span<MemberVariant> members)
        : ClassRegistrationBase(TypeId::ForType<T>(), &EnumInstance<T>::GetInstance(name, staticIndex, numDescendants, Name::Invalid(), attributes, flags, Span<MemberVariant>(members.Begin(), members.End())))
    {
#ifndef HYP_TOOL
        *pGlobal = m_class;
#endif
    }
};

} // namespace Hyperion
