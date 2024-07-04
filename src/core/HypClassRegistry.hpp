/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_REGISTRY_HPP
#define HYPERION_CORE_HYP_CLASS_REGISTRY_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

namespace hyperion {

class HypClass;

template <class T>
class HypClassInstance;

class HypClassProperty;

enum class HypClassFlags : uint32
{
    NONE = 0x0,
    ABSTRACT = 0x1,
    NO_DEFAULT_CONSTRUCTOR = 0x2,
    POD_TYPE = 0x4
};

HYP_MAKE_ENUM_FLAGS(HypClassFlags)

class HYP_API HypClassRegistry
{
public:
    static HypClassRegistry &GetInstance();

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or the null HypClass instance if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE
    const HypClass *GetClass() const
    {
        return GetClass(TypeID::ForType<NormalizedType<T>>());
    }

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \param type_id The type ID to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or the null HypClass instance if the type is not registered.
     */
    const HypClass *GetClass(TypeID type_id) const;

    /*! \brief Get the HypClass instance associated with the given name.
     *
     *  \param type_name The name of the type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or the null HypClass instance if the type is not registered.
     */
    const HypClass *GetClass(WeakName type_name) const;

    void RegisterClass(TypeID type_id, HypClass *hyp_class);

private:
    TypeMap<HypClass *> m_registered_classes;
};

namespace detail {

struct HYP_API HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class);
};

template <class T>
struct HypClassRegistration : public HypClassRegistrationBase
{   
    HypClassRegistration(EnumFlags<HypClassFlags> flags, Span<HypClassProperty> properties)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypClassInstance<T>::GetInstance(flags, Span<HypClassProperty>(properties.Begin(), properties.End())))
    {
    }
};

} // namespace detail
} // namespace hyperion

#endif