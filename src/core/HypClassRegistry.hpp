/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_REGISTRY_HPP
#define HYPERION_CORE_HYP_CLASS_REGISTRY_HPP

#include <core/Defines.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/utilities/Span.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/Name.hpp>

namespace hyperion {

class HypClass;

template <class T>
class HypClassInstance;

class HypClassProperty;

class HYP_API HypClassRegistry
{
public:
    static HypClassRegistry &GetInstance();

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    const HypClass *GetClass()
    {
        return GetClass(TypeID::ForType<NormalizedType<T>>());
    }

    HYP_NODISCARD
    const HypClass *GetClass(TypeID type_id);

    void RegisterClass(TypeID type_id, HypClass *hyp_class);

private:
    static const HypClass &GetNullHypClassInstance();

    TypeMap<HypClass *> m_registered_classes;
};

namespace detail {

struct HYP_API HypClassRegistrationBase
{
protected:
    TypeID      m_type_id;
    HypClass    *m_hyp_class;

    HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class);
};

template <class T>
struct HypClassRegistration : public HypClassRegistrationBase
{
    HypClassRegistration()
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypClassInstance<T>::GetInstance())
    {
    }
    
    HypClassRegistration(Span<HypClassProperty> properties)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypClassInstance<T>::GetInstance(Span<HypClassProperty>(properties.Begin(), properties.End())))
    {
    }
};

class NullHypClassInstance;

} // namespace detail
} // namespace hyperion

#endif