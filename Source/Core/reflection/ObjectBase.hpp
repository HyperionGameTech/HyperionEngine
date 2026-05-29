/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/ObjId.hpp>

#include <Core/threading/AtomicVar.hpp>

#include <Core/memory/allocator/Allocator.hpp>

#include <Core/debug/Debug.hpp>

#include <type_traits>

namespace Hyperion {

class Class;
class ScriptObjectResource;
struct ObjectHeader;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

namespace memory {
class Pool;
} // namespace memory

using memory::Pool;

namespace utilities {
struct TypeId;
} // namespace utilities

using utilities::TypeId;

#ifdef HYP_DOTNET
namespace dotnet {
class ManagedObject;
} // namespace dotnet
#endif

HYP_API extern bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);
HYP_API extern bool IsA(const Class* cls, const Class* instanceClass);

class HYP_API ObjectBase
{
    template <class T>
    friend struct Handle;

    template <class T>
    friend struct WeakHandle;

    friend struct AnyHandle;

    template <class T, class... Args>
    friend Handle<T> MakeHandle(Args&&...);

    template <class T>
    friend bool InitObject(T*);

public:
    struct ClassInfo
    {
        using Type = ObjectBase;
    };

    static Pool* GetAllocator();

    /*! \internal */
    ObjectBase();

    virtual ~ObjectBase();

    ObjIdBase Id() const;

    static const Class* StaticClass();
    const Class* InstanceClass() const;

    template <class TOther>
    bool IsA() const;
    bool IsA(const Class* cls) const;

    /*! \brief Manually call to add a new strong reference to this object. Do not use this method
     *  if using Handle<T> wrapper object as ref counts are managed automatically with it.
     *  \returns The strong reference count after being incremented */
    int32 AddRef();

    /*! \brief Manually call to release a strong reference to this object.
     *   Don't use this if using Handle wrappers as they manage ref counts automatically.
     *  \returns The strong reference count of the object after decrement */
    int32 Release();

    HYP_FORCE_INLINE ObjectHeader* GetObjectHeader_Internal() const
    {
        return m_header;
    }

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    void SetScriptObjectResource(ScriptObjectResource* scriptObjectResource)
    {
        HYP_CORE_ASSERT(m_scriptObjectResource == nullptr);

        m_scriptObjectResource = scriptObjectResource;
    }

    ScriptObjectResource* GetScriptObjectResource() const
    {
        return m_scriptObjectResource;
    }

#ifdef HYP_DOTNET
    dotnet::ManagedObject* GetManagedObject() const;
#endif
#endif

    HYP_FORCE_INLINE bool IsInitCalled() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_READY;
    }

protected:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED = 0x0,
        INIT_STATE_INIT_CALLED = 0x1,
        INIT_STATE_READY = 0x2
    };

    ObjectBase(const ObjectBase& other) = delete;
    ObjectBase& operator=(const ObjectBase& other) = delete;

    ObjectBase(ObjectBase&& other) noexcept
        : m_initState(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    ObjectBase& operator=(ObjectBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_initState.Set(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

        return *this;
    }

    /*! \brief Don't call manually (except for instances of derived types calling base class Init() in their own Init() method). */
    virtual void Init()
    {
        // Do nothing by default.
    }

    void SetReady(bool isReady)
    {
        if (isReady)
        {
            m_initState.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        }
        else
        {
            m_initState.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        HYP_CORE_ASSERT(IsReady(), "Object is not in ready state! Was InitObject() called for it?");
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        HYP_CORE_ASSERT(IsInitCalled(), "Object has not had Init() called on it!");
    }

    // Pointer to the header of the object, holding container, index and ref counts. Must be the first member.
    ObjectHeader* m_header;

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    ScriptObjectResource* m_scriptObjectResource;
#endif

private:
    // Used internally by InitObject() to call derived Init() methods.
    void Init_Internal()
    {
        Init();
    }

    AtomicVar<uint16> m_initState;
};

template <class TOther>
inline bool ObjectBase::IsA() const
{
    if constexpr (std::is_same_v<ObjectBase, TOther>)
    {
        return true;
    }
    else
    {
        return Hyperion::IsA(TOther::StaticClass(), InstanceClass());
    }
}

inline bool ObjectBase::IsA(const Class* cls) const
{
    return Hyperion::IsA(cls, InstanceClass());
}

} // namespace Hyperion
