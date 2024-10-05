/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_REGISTRY_HPP
#define HYPERION_CORE_HYP_CLASS_REGISTRY_HPP

#include <core/object/HypClassAttribute.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
} // namespace dotnet

class HypClass;
struct HypMember;

template <class T>
class HypClassInstance;

template <class T>
class HypStructInstance;

enum class HypClassFlags : uint32
{
    NONE        = 0x0,
    CLASS_TYPE  = 0x1,
    STRUCT_TYPE = 0x2,
    ABSTRACT    = 0x4
};

HYP_MAKE_ENUM_FLAGS(HypClassFlags)

class HYP_API HypClassRegistry
{
public:
    static HypClassRegistry &GetInstance();

    HypClassRegistry();
    HypClassRegistry(const HypClassRegistry &other)                 = delete;
    HypClassRegistry &operator=(const HypClassRegistry &other)      = delete;
    HypClassRegistry(HypClassRegistry &&other) noexcept             = delete;
    HypClassRegistry &operator=(HypClassRegistry &&other) noexcept  = delete;
    ~HypClassRegistry();

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or the null HypClass instance if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE const HypClass *GetClass() const
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

    void RegisterManagedClass(dotnet::Class *managed_class, const HypClass *hyp_class);
    void UnregisterManagedClass(dotnet::Class *managed_class);

    dotnet::Class *GetManagedClass(const HypClass *hyp_class) const;

    void Initialize();

private:
    TypeMap<HypClass *>                     m_registered_classes;

    bool                                    m_is_initialized;

    HashMap<HypClass *, dotnet::Class *>    m_managed_classes;
    mutable Mutex                           m_managed_classes_mutex;

    DataRaceDetector                        m_data_race_detector;
};

namespace detail {

struct HYP_API HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class);
};

template <class T, EnumFlags<HypClassFlags> Flags>
struct HypClassRegistration : public HypClassRegistrationBase
{   
    HypClassRegistration(Name name, Name parent_name, Span<HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypClassInstance<T>::GetInstance(name, parent_name, attributes, Flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T, EnumFlags<HypClassFlags> Flags>
struct HypStructRegistration : public HypClassRegistrationBase
{   
    HypStructRegistration(Name name, Span<HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypStructInstance<T>::GetInstance(name, {}, attributes, Flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

} // namespace detail
} // namespace hyperion

#endif