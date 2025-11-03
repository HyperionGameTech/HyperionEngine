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

class Class;
class ClassAttribute;
struct HypMember;

template <class T>
struct ClassDefinition;

template <class T>
class ClassInstance;

template <class T>
class StructInstance;

template <class T>
struct Handle;

HYP_API extern bool ClassRegistry_IsInitialized();

class HYP_API ClassRegistry
{
public:
    static ClassRegistry& GetInstance();

    ClassRegistry();
    ClassRegistry(const ClassRegistry& other) = delete;
    ClassRegistry& operator=(const ClassRegistry& other) = delete;
    ClassRegistry(ClassRegistry&& other) noexcept = delete;
    ClassRegistry& operator=(ClassRegistry&& other) noexcept = delete;
    ~ClassRegistry();

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_isInitialized;
    }

    /*! \brief Get the Class instance for the given type.
     *
     *  \tparam T The type to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE const Class* GetClass() const
    {
        static_assert(std::is_class_v<T> || std::is_enum_v<T>, "T must be an class or enum type to use GetClass<T>()");

        return GetClass(TypeId::ForType<NormalizedType<T>>());
    }

    /*! \brief Get the Class instance for the given type.
     *
     *  \param typeId The type Id to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered.
     */
    const Class* GetClass(TypeId typeId) const;

    /*! \brief Get the Class instance associated with the given name.
     *
     *  \param typeName The name of the type to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered.
     */
    const Class* GetClass(WeakName typeName) const;

    /*! \brief Get an enum Class instance associated with the given type.
     *
     *  \tparam T The type to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    template <class T>
    HYP_FORCE_INLINE const Class* GetEnum() const
    {
        static_assert(std::is_enum_v<T>, "T must be an enum type to use GetEnum<T>()");

        return GetEnum(TypeId::ForType<NormalizedType<T>>());
    }

    /*! \brief Get an enum Class instance associated with the given name.
     *
     *  \param typeId The type to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const Class* GetEnum(TypeId typeId) const;

    /*! \brief Get an enum Class instance associated with the given name.
     *
     *  \param typeId The type to get the Class instance for.
     *  \return The Class instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const Class* GetEnum(WeakName typeName) const;

    void RegisterClass(TypeId typeId, Class* cls);

    // Only for Dynamic classes
    bool UnregisterClass(const Class* cls);

    void ForEachClass(const ProcRef<IterationResult(const Class*)>& callback, bool includeDynamicClasses = true) const;

    void Initialize();

private:
    Array<Class*> m_classesByStaticIndex;

    HashMap<TypeId, Class*> m_classesByTypeId;

    mutable Mutex m_mutex;
    HashMap<TypeId, Class*> m_dynamicClasses;

    HashMap<Class*, RC<dotnet::ManagedClass>> m_managedClasses;
    HashMap<dotnet::ManagedClass*, Class*> m_managedClassesReverseMapping;
    mutable Mutex m_managedClassesMutex;

    bool m_isInitialized : 1;
};

} // namespace hyperion
