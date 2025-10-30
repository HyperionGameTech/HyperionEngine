/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/TypeId.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/ForEach.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/functional/Proc.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class ManagedClass;
class ManagedObject;
} // namespace dotnet

class HypClass;
class HypClassAttribute;
struct HypMember;

template <class T>
struct HypClassDefinition;

template <class T>
class HypClassInstance;

template <class T>
class HypStructInstance;

template <class T>
struct Handle;

HYP_API extern bool HypClassRegistry_IsInitialized();

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

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_isInitialized;
    }

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE const HypClass* GetClass() const
    {
        static_assert(std::is_class_v<T> || std::is_enum_v<T>, "T must be an class or enum type to use GetClass<T>()");

        return GetClass(TypeId::ForType<NormalizedType<T>>());
    }

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \param typeId The type Id to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(TypeId typeId) const;

    /*! \brief Get the HypClass instance associated with the given name.
     *
     *  \param typeName The name of the type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(WeakName typeName) const;

    /*! \brief Get an enum HypClass instance associated with the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    template <class T>
    HYP_FORCE_INLINE const HypClass* GetEnum() const
    {
        static_assert(std::is_enum_v<T>, "T must be an enum type to use GetEnum<T>()");

        return GetEnum(TypeId::ForType<NormalizedType<T>>());
    }

    /*! \brief Get an enum HypClass instance associated with the given name.
     *
     *  \param typeId The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypClass* GetEnum(TypeId typeId) const;

    /*! \brief Get an enum HypClass instance associated with the given name.
     *
     *  \param typeId The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypClass* GetEnum(WeakName typeName) const;

    void RegisterClass(TypeId typeId, HypClass* hypClass);

    // Only for Dynamic classes
    bool UnregisterClass(const HypClass* hypClass);

    void ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool includeDynamicClasses = true) const;

    void Initialize();

private:
    HashMap<TypeId, HypClass*> m_registeredClasses;

    mutable Mutex m_mutex;
    HashMap<TypeId, HypClass*> m_dynamicClasses;

    HashMap<HypClass*, RC<dotnet::ManagedClass>> m_managedClasses;
    HashMap<dotnet::ManagedClass*, HypClass*> m_managedClassesReverseMapping;
    mutable Mutex m_managedClassesMutex;

    bool m_isInitialized : 1;
};

} // namespace hyperion
