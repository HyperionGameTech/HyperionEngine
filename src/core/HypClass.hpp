/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/HypClassRegistry.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/Span.hpp>

namespace hyperion {

class HypClassProperty;

class HYP_API HypClass
{
public:
    friend struct detail::HypClassRegistrationBase;

    HypClass(TypeID type_id, Span<HypClassProperty> properties);
    HypClass(const HypClass &other)                 = delete;
    HypClass &operator=(const HypClass &other)      = delete;
    HypClass(HypClass &&other) noexcept             = delete;
    HypClass &operator=(HypClass &&other) noexcept  = delete;
    virtual ~HypClass();

    HYP_NODISCARD
    virtual Name GetName() const = 0;

    HYP_NODISCARD HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return m_type_id; }

    HYP_NODISCARD
    virtual bool IsValid() const
        { return false; }

    HYP_NODISCARD
    HypClassProperty *GetProperty(Name name) const;

    HYP_NODISCARD HYP_FORCE_INLINE
    const Array<HypClassProperty *> &GetProperties() const
        { return m_properties; }

protected:
    TypeID                              m_type_id;
    Array<HypClassProperty *>           m_properties;
    HashMap<Name, HypClassProperty *>   m_properties_by_name;
};

template <class T>
class HypClassInstance : public HypClass
{
public:
    static HypClassInstance &GetInstance(Span<HypClassProperty> properties)
    {
        static HypClassInstance instance { properties };

        return instance;
    }

    HypClassInstance(Span<HypClassProperty> properties)
        : HypClass(TypeID::ForType<T>(), properties)
    {
    }

    virtual ~HypClassInstance() = default;

    virtual Name GetName() const override
    {
        static const Name name = CreateNameFromDynamicString(TypeNameWithoutNamespace<T>().Data());

        return name;
    }

    virtual bool IsValid() const override
    {
        return true;
    }
};

// Bad class result (not registered)
template <>
class HypClassInstance<void>;

} // namespace hyperion

#endif