/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/Stack.hpp>
#include <Core/containers/TypeMap.hpp>

#include <Core/memory/Memory.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/threading/Thread.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace utilities {

class GlobalContextRegistry;

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread();

class GlobalContextHolderBase
{
    friend class GlobalContextRegistry;

protected:
    GlobalContextHolderBase()
        : m_pFnDestructor(nullptr)
    {
    }

public:
    ~GlobalContextHolderBase()
    {
        for (SizeType i = m_stack.Size(); i > 0; i--)
        {
            if (m_pFnDestructor != nullptr)
            {
                m_pFnDestructor(m_stack[i - 1]);
            }

            free(m_stack[i - 1]);
        }
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_stack.Size();
    }

    void Pop()
    {
        HYP_CORE_ASSERT(m_stack.Size() > 0);

        if (m_pFnDestructor != nullptr)
        {
            m_pFnDestructor(m_stack.Back());
        }

        free(m_stack.PopBack());
    }

protected:
    Array<void*> m_stack;
    void (*m_pFnDestructor)(void*);
};

template <class ContextType>
class GlobalContextHolder;

class HYP_API GlobalContextRegistry
{
public:
    GlobalContextRegistry();
    GlobalContextRegistry(const GlobalContextRegistry& other) = delete;
    GlobalContextRegistry& operator=(const GlobalContextRegistry& other) = delete;
    GlobalContextRegistry(GlobalContextRegistry&& other) noexcept = delete;
    GlobalContextRegistry& operator=(GlobalContextRegistry&& other) noexcept = delete;
    ~GlobalContextRegistry();

    template <class T>
    HYP_FORCE_INLINE GlobalContextHolder<T>& GetOrCreateContextHolder()
    {
        auto it = m_contextHolders.Find<T>();

        if (it == m_contextHolders.End())
        {
            it = m_contextHolders.Set<T>(new GlobalContextHolder<T>()).first;
        }

        return *static_cast<GlobalContextHolder<T>*>(it->second);
    }

    template <class T>
    HYP_FORCE_INLINE GlobalContextHolder<T>* GetContextHolder()
    {
        auto it = m_contextHolders.Find<T>();

        if (it == m_contextHolders.End())
        {
            return nullptr;
        }

        return static_cast<GlobalContextHolder<T>*>(it->second);
    }

private:
    ThreadId m_ownerThreadId;
    TypeMap<GlobalContextHolderBase*> m_contextHolders;
};

template <class ContextType>
class GlobalContextHolder final : public GlobalContextHolderBase
{
public:
    GlobalContextHolder() = default;

    GlobalContextHolder(const GlobalContextHolder& other) = delete;
    GlobalContextHolder& operator=(const GlobalContextHolder& other) = delete;

    GlobalContextHolder(GlobalContextHolder&& other) noexcept = delete;
    GlobalContextHolder& operator=(GlobalContextHolder&& other) noexcept = delete;

    static GlobalContextHolder& GetInstance()
    {
        thread_local GlobalContextHolder<ContextType>& result = GetGlobalContextRegistryForCurrentThread()->GetOrCreateContextHolder<ContextType>();

        return result;
    }

    static GlobalContextHolder* GetInstanceIfNotNull()
    {
        return GetGlobalContextRegistryForCurrentThread()->GetContextHolder<ContextType>();
    }

    template <class... Args>
    void Push(Args&&... args)
    {
        void* mem = malloc(sizeof(ContextType));
        HYP_CORE_ASSERT(mem != nullptr);

        m_stack.PushBack(new (mem) ContextType(std::forward<Args>(args)...));

        if constexpr (!std::is_trivially_destructible_v<ContextType>)
        {
            if (HYP_UNLIKELY(m_pFnDestructor == nullptr))
            {
                // set destructor on first call to Push() - that way we can use undefined class in places to check if a context is active.
                m_pFnDestructor = &Memory::Destruct<ContextType>;
            }
        }
    }

    ContextType& Current()
    {
        HYP_CORE_ASSERT(m_stack.Size() > 0);

        return *reinterpret_cast<ContextType*>(m_stack.Back());
    }
};

struct GlobalContextScope
{
    GlobalContextHolderBase* holder;

    template <class ContextType>
    GlobalContextScope(ContextType&& context)
        : holder(&GlobalContextHolder<NormalizedType<ContextType>>::GetInstance())
    {
        static_cast<GlobalContextHolder<NormalizedType<ContextType>>*>(holder)->Push(std::forward<ContextType>(context));
    }

    GlobalContextScope(const GlobalContextScope& other) = delete;
    GlobalContextScope& operator=(const GlobalContextScope& other) = delete;

    GlobalContextScope(GlobalContextScope&& other) noexcept = delete;
    GlobalContextScope& operator=(GlobalContextScope&& other) noexcept = delete;

    ~GlobalContextScope()
    {
        holder->Pop();
    }
};

template <class ContextType>
static inline bool IsGlobalContextActive()
{
    GlobalContextHolder<ContextType>* holder = GlobalContextHolder<ContextType>::GetInstanceIfNotNull();

    return holder && holder->Size() != 0;
}

template <class ContextType>
static inline ContextType* GetGlobalContext()
{
    GlobalContextHolder<ContextType>& holder = GlobalContextHolder<ContextType>::GetInstance();

    if (holder.Size() != 0)
    {
        return &holder.Current();
    }

    return nullptr;
}

template <class ContextType>
static inline void PushGlobalContext(ContextType&& context)
{
    GlobalContextHolder<ContextType>& holder = GlobalContextHolder<ContextType>::GetInstance();

    holder.Push(std::forward<ContextType>(context));
}

template <class ContextType>
static inline ContextType PopGlobalContext()
{
    GlobalContextHolder<ContextType>& holder = GlobalContextHolder<ContextType>::GetInstance();

    ContextType current = std::move(holder.Current());
    holder.Pop();

    return current;
}

} // namespace utilities

using utilities::GetGlobalContext;
using utilities::GlobalContextScope;
using utilities::IsGlobalContextActive;
using utilities::PopGlobalContext;
using utilities::PushGlobalContext;

} // namespace Hyperion
