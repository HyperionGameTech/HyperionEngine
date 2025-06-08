/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_REGISTRY_HPP
#define HYPERION_CORE_HYP_CLASS_REGISTRY_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/ForEach.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
} // namespace dotnet

class HypClass;
class HypClassAttribute;
class HypEnum;
struct HypMember;

template <class T>
struct HypClassDefinition;

template <class THypClassDefinition>
class HypClassInstance;

template <class THypClassDefinition>
class HypStructInstance;

template <class THypClassDefinition>
class HypEnumInstance;

template <class T>
struct Handle;

enum class HypClassFlags : uint32
{
    NONE = 0x0,
    CLASS_TYPE = 0x1,
    STRUCT_TYPE = 0x2,
    ENUM_TYPE = 0x4,
    ABSTRACT = 0x8,
    POD_TYPE = 0x10,
    DYNAMIC = 0x20 // Dynamic classes are not registered in the class registry
};

HYP_MAKE_ENUM_FLAGS(HypClassFlags)

class HYP_API HypClassRegistry
{
public:
    static HypClassRegistry& GetInstance();

    HypClassRegistry();
    HypClassRegistry(const HypClassRegistry& other) = delete;
    HypClassRegistry& operator=(const HypClassRegistry& other) = delete;
    HypClassRegistry(HypClassRegistry&& other) noexcept = delete;
    HypClassRegistry& operator=(HypClassRegistry&& other) noexcept = delete;
    ~HypClassRegistry();

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE const HypClass* GetClass() const
    {
        static_assert(std::is_class_v<T> || std::is_enum_v<T>, "T must be an class or enum type to use GetClass<T>()");

        static constexpr TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        return GetClass(type_id);
    }

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \param type_id The type ID to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(TypeID type_id) const;

    /*! \brief Get the HypClass instance associated with the given name.
     *
     *  \param type_name The name of the type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(WeakName type_name) const;

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    template <class T>
    HYP_FORCE_INLINE const HypEnumInstance<T>* GetEnum() const
    {
        static_assert(std::is_enum_v<T>, "T must be an enum type to use GetEnum<T>()");

        static constexpr TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        return static_cast<const HypEnumInstance<T>*>(GetEnum(type_id));
    }

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \param type_id The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypEnum* GetEnum(TypeID type_id) const;

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \param type_id The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypEnum* GetEnum(WeakName type_name) const;

    void RegisterClass(TypeID type_id, HypClass* hyp_class);

    // Only for Dynamic classes
    void UnregisterClass(const HypClass* hyp_class);

    void ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool include_dynamic_classes = true) const;

    void RegisterManagedClass(const RC<dotnet::Class>& managed_class, const HypClass* hyp_class);
    void UnregisterManagedClass(dotnet::Class* managed_class);

    RC<dotnet::Class> GetManagedClass(const HypClass* hyp_class) const;

    void Initialize();

private:
    TypeMap<HypClass*> m_registered_classes;

    mutable Mutex m_dynamic_classes_mutex;
    TypeMap<HypClass*> m_dynamic_classes;

    bool m_is_initialized;

    HashMap<HypClass*, RC<dotnet::Class>> m_managed_classes;
    HashMap<dotnet::Class*, HypClass*> m_managed_classes_reverse_mapping;
    mutable Mutex m_managed_classes_mutex;
};

struct HYP_API HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeID type_id, HypClass* hyp_class);
};

template <class T>
struct HypClassRegistration : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::CLASS_TYPE
        | (is_pod_type<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypClassRegistration(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypClassInstance<T>::GetInstance(name, static_index, num_descendants, parent_name, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypStructRegistration : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::STRUCT_TYPE
        | (is_pod_type<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypStructRegistration(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypStructInstance<T>::GetInstance(name, static_index, num_descendants, parent_name, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypEnumRegistration : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::ENUM_TYPE;

    HypEnumRegistration(Name name, int static_index, uint32 num_descendants, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeID::ForType<T>(), &HypEnumInstance<T>::GetInstance(name, static_index, num_descendants, Name::Invalid(), attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

#pragma region Global Helper Functions

template <class T>
HYP_FORCE_INLINE const HypClass* GetClass()
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

template <class T>
HYP_FORCE_INLINE const HypClass* GetClass(const T* ptr)
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

template <class T>
HYP_FORCE_INLINE const HypClass* GetClass(const Handle<T>& handle)
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

HYP_API const HypClass* GetClass(TypeID type_id);
HYP_API const HypClass* GetClass(WeakName type_name);

template <class T>
HYP_FORCE_INLINE const HypEnumInstance<T>* GetEnum()
{
    return HypClassRegistry::GetInstance().template GetEnum<T>();
}

HYP_API const HypEnum* GetEnum(TypeID type_id);
HYP_API const HypEnum* GetEnum(WeakName type_name);

HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id);
HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const HypClass* instance_hyp_class);

#pragma endregion Global Helper Functions

} // namespace hyperion

#endif