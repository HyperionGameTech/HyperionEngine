/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/utilities/StringView.hpp>
#include <Core/utilities/EnumFlags.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/DataRaceDetector.hpp>

#include <dotnet/Helpers.hpp>

#include <type_traits>

#define HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE

namespace Hyperion {

enum class ObjectFlags : uint32
{
    NONE = 0x0,
    CREATED_FROM_MANAGED = 0x1
};

HYP_MAKE_ENUM_FLAGS(ObjectFlags)

} // namespace Hyperion

namespace Hyperion::dotnet {

class ManagedClass;
class ManagedObject;
class Assembly;
class ManagedMethod;
class ManagedProperty;

struct ObjectReference
{
    void* weakHandle;
    void* strongHandle;

    bool operator==(const ObjectReference& other) const = default;
    bool operator!=(const ObjectReference& other) const = default;
};

static_assert(sizeof(ObjectReference) == 16, "ObjectReference size mismatch with C#");

/*! \brief References a managed object in the .NET runtime.
 *  By default, the managed object this ManagedObject is associated with will be allowed to be released by the .NET runtime upon this object's destruction.
 *  To allow the managed object to live beyond the lifetime of this object, use the ObjectFlags::CREATED_FROM_MANAGED flag.
 *
 *  \details To create a new ManagedObject, use the ManagedClass::NewObject method.
 * */
class HYP_API ManagedObject final
{
public:
    ManagedObject();
    ManagedObject(const RC<ManagedClass>& managedClass, ObjectReference objectReference, EnumFlags<ObjectFlags> objectFlags = ObjectFlags::NONE);

    ManagedObject(const ManagedObject&) = delete;
    ManagedObject& operator=(const ManagedObject&) = delete;

    ManagedObject(ManagedObject&& other) noexcept = delete;
    ManagedObject& operator=(ManagedObject&& other) noexcept = delete;

    // Destructor frees the managed object unless CREATED_FROM_MANAGED is set.
    ~ManagedObject();

    HYP_FORCE_INLINE const RC<ManagedClass>& GetClass() const
    {
        return m_managedClass;
    }

    HYP_FORCE_INLINE const ObjectReference& GetObjectReference() const
    {
        return m_objectReference;
    }

    HYP_FORCE_INLINE EnumFlags<ObjectFlags> GetObjectFlags() const
    {
        return m_objectFlags;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_objectReference.weakHandle != nullptr;
    }

    /*! \brief Is the object set to be kept alive?
     *  \details If true, the managed object will not be garbage collected by the .NET runtime.
     *  \internal Use this function only for debugging
     *  \return True if the object is set to be kept alive, false otherwise */
    HYP_FORCE_INLINE bool ShouldKeepAlive() const
    {
        return m_keepAlive.Get(MemoryOrder::ACQUIRE);
    }

    /*! \brief Set whether or not the managed object should be kept in memory (not garbage collected)
     *  \param keepAlive Whether or not to allow the object to exist in memory persistently */
    bool SetKeepAlive(bool keepAlive);

    const ManagedMethod* GetMethod(ANSIStringView methodName) const;

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const ManagedMethod* pMethod, Args&&... args)
    {
        return InvokeMethod_CheckArgs<ReturnType>(pMethod, std::forward<Args>(args)...);
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE ReturnType InvokeMethodByName(ANSIStringView methodName, Args&&... args)
    {
        Assert(IsValid());

        const ManagedMethod* pMethod = GetMethod(methodName);
        Assert(pMethod != nullptr, "Method {} not found", methodName);

        return InvokeMethod_CheckArgs<ReturnType>(pMethod, std::forward<Args>(args)...);
    }

private:
    /*! \brief Reset the ManagedObject to an invalid state.
     *  This will free the managed object if it is still alive unless the ObjectFlags::CREATED_FROM_MANAGED flag is set.
     * */
    void Reset();

    void InvokeMethod_Internal(const ManagedMethod* pMethod, const BoxedValue** argsBoxed, BoxedValue* outBoxed);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod_CheckArgs(const ManagedMethod* pMethod, Args&&... args)
    {
        if constexpr (sizeof...(args) != 0)
        {
            BoxedValue* argsArray = (BoxedValue*)StackAlloc(sizeof(BoxedValue) * sizeof...(args));
            const BoxedValue* argsArrayPtr[sizeof...(args) + 1]; // Mark last as nullptr so C# can use it as a null terminator

            SetArgsBoxed(std::make_index_sequence<sizeof...(args)>(), argsArray, argsArrayPtr, std::forward<Args>(args)...);

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeMethod_Internal(pMethod, argsArrayPtr, nullptr);
            }
            else
            {
                BoxedValue boxed;
                InvokeMethod_Internal(pMethod, argsArrayPtr, &boxed);

                if (boxed.IsNull())
                {
                    return ReturnType();
                }

                return std::move(boxed.Get<ReturnType>());
            }
        }
        else
        {
            const BoxedValue* argsArrayPtr[] = { nullptr };

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeMethod_Internal(pMethod, argsArrayPtr, nullptr);
            }
            else
            {
                BoxedValue boxed;
                InvokeMethod_Internal(pMethod, argsArrayPtr, &boxed);

                if (boxed.IsNull())
                {
                    return ReturnType();
                }

                return std::move(boxed.Get<ReturnType>());
            }
        }
    }

    const ManagedProperty* GetProperty(ANSIStringView methodName) const;

    RC<ManagedClass> m_managedClass;
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    RC<Assembly> m_assembly; // Keep a reference to the assembly to prevent it from being unloaded while this object is alive.
#else
    Weak<Assembly> m_assembly;
#endif
    ObjectReference m_objectReference;
    EnumFlags<ObjectFlags> m_objectFlags;

    AtomicVar<bool> m_keepAlive;
};

} // namespace Hyperion::dotnet
