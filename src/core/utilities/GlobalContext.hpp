/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GLOBAL_CONTEXT_HPP
#define HYPERION_GLOBAL_CONTEXT_HPP

#include <core/containers/Stack.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Thread.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace utilities {

class GlobalContextRegistry;

HYP_API GlobalContextRegistry *GetGlobalContextRegistryForCurrentThread();

class IGlobalContextHolder
{
public:
    virtual ~IGlobalContextHolder() = default;

    virtual SizeType Size() const = 0;
    virtual void Pop() = 0;
};

template <class ContextType>
class GlobalContextHolder;

class GlobalContextRegistry
{
public:
    HYP_API GlobalContextRegistry();
    GlobalContextRegistry(const GlobalContextRegistry &other)                   = delete;
    GlobalContextRegistry &operator=(const GlobalContextRegistry &other)        = delete;
    GlobalContextRegistry(GlobalContextRegistry &&other) noexcept               = delete;
    GlobalContextRegistry &operator=(GlobalContextRegistry &&other) noexcept    = delete;
    HYP_API ~GlobalContextRegistry();

    template <class T>
    HYP_FORCE_INLINE GlobalContextHolder<T> &GetContextHolder()
    {
        auto it = m_context_holders.Find<T>();

        if (it == m_context_holders.End()) {
            it = m_context_holders.Set<T>(MakeUnique<GlobalContextHolder<T>>());
        }

        return *static_cast<GlobalContextHolder<T> *>(it->second.Get());
    }

private:
    ThreadID                                    m_owner_thread_id;
    TypeMap<UniquePtr<IGlobalContextHolder>>    m_context_holders;
};

template <class ContextType>
class GlobalContextHolder final : public IGlobalContextHolder
{
private:
    GlobalContextHolder() = default;

public:
    friend class GlobalContextRegistry;

    GlobalContextHolder(const GlobalContextHolder &other)                   = delete;
    GlobalContextHolder &operator=(const GlobalContextHolder &other)        = delete;
    GlobalContextHolder(GlobalContextHolder &&other) noexcept               = delete;
    GlobalContextHolder &operator=(GlobalContextHolder &&other) noexcept    = delete;
    ~GlobalContextHolder()                                                  = default;

    static GlobalContextHolder &GetInstance()
    {
        static GlobalContextHolder<ContextType> &result = GetGlobalContextRegistryForCurrentThread()->GetContextHolder<ContextType>();

        return result;
    }

    virtual SizeType Size() const override
    {
        return m_contexts.Size();
    }

    void Push(ContextType &&context)
    {
        m_contexts.Push(std::move(context));
    }

    virtual void Pop() override
    {
        m_contexts.Pop();
    }

    ContextType &Current()
    {
        return m_contexts.Top();
    }

private:
    Stack<ContextType> m_contexts;
};

struct GlobalContextScope
{
    IGlobalContextHolder    *holder;

    template <class ContextType>
    GlobalContextScope(ContextType &&context)
        : holder(&GlobalContextHolder<ContextType>::GetInstance())
    {
        static_cast<GlobalContextHolder<ContextType> *>(holder)->Push(std::forward<ContextType>(context));
    }

    GlobalContextScope(const GlobalContextScope &other)                 = delete;
    GlobalContextScope &operator=(const GlobalContextScope &other)      = delete;

    GlobalContextScope(GlobalContextScope &&other) noexcept             = delete;
    GlobalContextScope &operator=(GlobalContextScope &&other) noexcept  = delete;

    ~GlobalContextScope()
    {
        holder->Pop();
    }
};

template <class ContextType, class FunctionType, class ReturnType>
static inline ReturnType UseGlobalContext(FunctionType &&func)
{
    GlobalContextHolder<ContextType> &holder = &GlobalContextHolder<ContextType>::GetInstance();
    AssertThrow(holder.Size() != 0);

    return func(holder.Current());
}

} // namespace utilities

using utilities::GlobalContextScope;
using utilities::UseGlobalContext;

} // namespace hyperion

#endif