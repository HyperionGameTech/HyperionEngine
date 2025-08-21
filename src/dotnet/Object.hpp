/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <dotnet/interop/ManagedObject.hpp>
#include <dotnet/Helpers.hpp>

#include <type_traits>

#define HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE

namespace hyperion {

enum class ObjectFlags : uint32
{
    NONE = 0x0,
    CREATED_FROM_MANAGED = 0x1
};

HYP_MAKE_ENUM_FLAGS(ObjectFlags)

} // namespace hyperion

namespace hyperion::dotnet {

class Class;
class Object;
class Assembly;
class Method;
class Property;

/*! \brief References a managed object in the .NET runtime.
 *  By default, the managed object this Object is associated with will be allowed to be released by the .NET runtime upon this object's destruction.
 *  To allow the managed object to live beyond the lifetime of this object, use the ObjectFlags::CREATED_FROM_MANAGED flag.
 *
 *  \details To create a new Object, use the Class::NewObject method.
 * */
class HYP_API Object final
{
public:
    Object();
    Object(const RC<Class>& classPtr, ObjectReference objectReference, EnumFlags<ObjectFlags> objectFlags = ObjectFlags::NONE);

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Object(Object&& other) noexcept = delete;
    Object& operator=(Object&& other) noexcept = delete;

    // Destructor frees the managed object unless CREATED_FROM_MANAGED is set.
    ~Object();

    HYP_FORCE_INLINE const RC<Class>& GetClass() const
    {
        return m_classPtr;
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

    const Method* GetMethod(UTF8StringView methodName) const;

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const Method* methodPtr, Args&&... args)
    {
        return InvokeMethod_CheckArgs<ReturnType>(methodPtr, std::forward<Args>(args)...);
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE ReturnType InvokeMethodByName(UTF8StringView methodName, Args&&... args)
    {
        Assert(IsValid());

        const Method* methodPtr = GetMethod(methodName);
        Assert(methodPtr != nullptr, "Method {} not found", methodName);

        return InvokeMethod_CheckArgs<ReturnType>(methodPtr, std::forward<Args>(args)...);
    }

private:
    /*! \brief Reset the Object to an invalid state.
     *  This will free the managed object if it is still alive unless the ObjectFlags::CREATED_FROM_MANAGED flag is set.
     * */
    void Reset();

    void InvokeMethod_Internal(const Method* methodPtr, const HypData** argsHypData, HypData* outReturnHypData);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod_CheckArgs(const Method* methodPtr, Args&&... args)
    {
        if constexpr (sizeof...(args) != 0)
        {
            HypData* argsArray = (HypData*)StackAlloc(sizeof(HypData) * sizeof...(args));
            const HypData* argsArrayPtr[sizeof...(args) + 1]; // Mark last as nullptr so C# can use it as a null terminator

            SetArgs_HypData(std::make_index_sequence<sizeof...(args)>(), argsArray, argsArrayPtr, std::forward<Args>(args)...);

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                HypData returnHypData;
                InvokeMethod_Internal(methodPtr, argsArrayPtr, &returnHypData);

                if (returnHypData.IsNull())
                {
                    return ReturnType();
                }

                return std::move(returnHypData.Get<ReturnType>());
            }
        }
        else
        {
            const HypData* argsArrayPtr[] = { nullptr };

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                HypData returnHypData;
                InvokeMethod_Internal(methodPtr, argsArrayPtr, &returnHypData);

                if (returnHypData.IsNull())
                {
                    return ReturnType();
                }

                return std::move(returnHypData.Get<ReturnType>());
            }
        }
    }

    const Property* GetProperty(UTF8StringView methodName) const;

    RC<Class> m_classPtr;
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    RC<Assembly> m_assembly; // Keep a reference to the assembly to prevent it from being unloaded while this object is alive.
#else
    Weak<Assembly> m_assembly;
#endif
    ObjectReference m_objectReference;
    EnumFlags<ObjectFlags> m_objectFlags;

    AtomicVar<bool> m_keepAlive;
};

} // namespace hyperion::dotnet
