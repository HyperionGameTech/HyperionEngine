/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/TypeId.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/ForEach.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Containers/TypeMap.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Defines.hpp>
#include <Core/Name/Name.hpp>
#include <Core/Util.hpp>

#include <type_traits>

namespace Hyperion {

namespace dotnet {
class ManagedClass;
class ManagedObject;
} // namespace dotnet

class Class;
class ClassAttribute;
struct MemberVariant;

template <class T>
struct ClassDefinition;

template <class T>
class ClassInstance;

template <class T>
class StructInstance;

template <class T>
struct Handle;

CORE_API extern bool ClassRegistry_IsInitialized();

class CORE_API ClassRegistry
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
    const Class* GetClass(StringHash typeName) const;

    /*! \brief Get the Class instance associated with the given name, with an option to ignore case.
     *  Heavier than the StringHash version, so only use if necessary
     */
    const Class* GetClass(ANSIStringView typeName, bool ignoreCase) const;

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
    const Class* GetEnum(StringHash typeName) const;

    /*! \brief Register a class instance with the class registry for global use.
    *   \param typeId The typeId to associate with the Class instance pointer
    *   \param cls The class instance pointer
    *   \param [outWasRegistered] If provided, will be set to true/false depending on whether or not registration succeeded
    *                             Otherwise, ignored. (The class instance will not be registered if another one exists for the given \p{typeId}  */
    void Register(TypeId typeId, Class* cls, bool* outWasRegistered = nullptr);

    // Only for Dynamic classes
    bool Unregister(const Class* cls);

    void ForEachClass(const ProcRef<IterationResult(const Class*)>& callback, bool includeDynamicClasses = true) const;

    void Initialize();

private:
    Array<Class*> m_classesByStaticIndex;

    Map<TypeId, Class*> m_classesByTypeId;

    mutable Mutex m_mutex;
    Map<TypeId, Class*> m_dynamicClasses;

    Map<Class*, SharedPtr<dotnet::ManagedClass>> m_managedClasses;
    Map<dotnet::ManagedClass*, Class*> m_managedClassesReverseMapping;
    mutable Mutex m_managedClassesMutex;

    bool m_isInitialized : 1;
};

} // namespace Hyperion
