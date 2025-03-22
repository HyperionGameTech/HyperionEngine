/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_OBJECT_HPP
#define HYPERION_DOTNET_OBJECT_HPP

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>

#include <dotnet/Method.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <type_traits>

// #define HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE

namespace hyperion {

enum class ObjectFlags : uint32
{
    NONE                    = 0x0,
    CREATED_FROM_MANAGED    = 0x1,
    KEEP_ALIVE              = 0x2
};

HYP_MAKE_ENUM_FLAGS(ObjectFlags)

} // namespace hyperion

namespace hyperion::dotnet {

class Class;
class Object;
class Assembly;
class Method;
class Property;

/*! \brief A move-only object that represents a managed object in the .NET runtime.
 *  By default, the managed object this Object is associated with will be allowed to be released by the .NET runtime upon this object's destruction.
 *  To allow the managed object to live beyond the lifetime of this object, use the ObjectFlags::CREATED_FROM_MANAGED flag.
 * 
 *  \details To create a new Object, use the Class::NewObject method.
 * */
class HYP_API Object
{
public:
    Object();
    Object(const RC<Class> &class_ptr, ObjectReference object_reference, EnumFlags<ObjectFlags> object_flags = ObjectFlags::NONE);

    Object(const Object &)                      = delete;
    Object &operator=(const Object &)           = delete;

    Object(Object &&other) noexcept             = delete;
    Object &operator=(Object &&other) noexcept  = delete;

    // Destructor frees the managed object unless CREATED_FROM_MANAGED is set.
    ~Object();

    HYP_FORCE_INLINE const RC<Class> &GetClass() const
        { return m_class_ptr; }

    HYP_FORCE_INLINE const ObjectReference &GetObjectReference() const
        { return m_object_reference; }

    HYP_FORCE_INLINE ObjectFlags GetObjectFlags() const
        { return m_object_flags; }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_object_reference.guid.IsValid(); }

    /*! \brief Set whether or not the managed object should be kept in memory (not garbage collected)
     *  Defaults to alive for an object created from the unmanaged side. For objects created from the managed runtime,
     *  use this method to allow it to persist beyond the initial scope.
     *  \param keep_alive Whether or not to allow the object to exist in memory persistently */
    bool SetKeepAlive(bool keep_alive);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const Method *method_ptr, Args &&... args)
    {
        return InvokeMethod_CheckArgs<ReturnType>(method_ptr, std::forward<Args>(args)...);
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE ReturnType InvokeMethodByName(UTF8StringView method_name, Args &&... args)
    {
        AssertThrow(IsValid());

        const Method *method_ptr = GetMethod(method_name);
        AssertThrowMsg(method_ptr != nullptr, "Method %s not found", method_name.Data());

        return InvokeMethod_CheckArgs<ReturnType>(method_ptr, std::forward<Args>(args)...);
    }

private:
    /*! \brief Reset the Object to an invalid state.
    *  This will free the managed object if it is still alive unless the ObjectFlags::CREATED_FROM_MANAGED flag is set.
    * */
    void Reset();

    const Method *GetMethod(UTF8StringView method_name) const;

    void InvokeMethod_Internal(const Method *method_ptr, const HypData **args_hyp_data, HypData *out_return_hyp_data);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod_CheckArgs(const Method *method_ptr, Args &&... args)
    {
        if constexpr (sizeof...(args) != 0) {
            HypData *args_array = (HypData *)StackAlloc(sizeof(HypData) * sizeof...(args));
            const HypData *args_array_ptr[sizeof...(args)];

            FillArgs_HypData(std::make_index_sequence<sizeof...(args)>(), args_array, args_array_ptr, std::forward<Args>(args)...);

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, &args_array_ptr[0], nullptr);
            } else {
                HypData return_hyp_data;
                InvokeMethod_Internal(method_ptr, &args_array_ptr[0], &return_hyp_data);

                if (return_hyp_data.IsNull()) {
                    return ReturnType();
                }

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, nullptr, nullptr);
            } else {
                HypData return_hyp_data;
                InvokeMethod_Internal(method_ptr, nullptr, &return_hyp_data);

                if (return_hyp_data.IsNull()) {
                    return ReturnType();
                }

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        }
    }

    const Property *GetProperty(UTF8StringView method_name) const;

    RC<Class>               m_class_ptr;
#ifdef HYP_DOTNET_OBJECT_KEEP_ASSEMBLY_ALIVE
    RC<Assembly>            m_assembly; // Keep a reference to the assembly to prevent it from being unloaded while this object is alive.
#else
    Weak<Assembly>          m_assembly;
#endif
    ObjectReference         m_object_reference;
    EnumFlags<ObjectFlags>  m_object_flags;
};

} // namespace hyperion::dotnet

#endif