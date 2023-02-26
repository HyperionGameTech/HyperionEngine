#ifndef STACK_TRACE_HPP
#define STACK_TRACE_HPP

#include <core/Core.hpp>

struct StackTrace
{
    int call_addresses[10];

    StackTrace()
    {
        hyperion::Memory::MemSet(call_addresses, 0, sizeof(call_addresses));
    }

    StackTrace(const StackTrace &other)
    {
        hyperion::Memory::MemCpy(call_addresses, other.call_addresses, sizeof(call_addresses));
    }

    StackTrace &operator=(const StackTrace &other)
    {
        hyperion::Memory::MemCpy(call_addresses, other.call_addresses, sizeof(call_addresses));

        return *this;
    }

    ~StackTrace() = default;
};

#endif