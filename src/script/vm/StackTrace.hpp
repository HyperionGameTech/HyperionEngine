#ifndef STACK_TRACE_HPP
#define STACK_TRACE_HPP

#include <cstring>

struct StackTrace {
    int call_addresses[10];

    StackTrace()
    {
        std::memset(call_addresses, 0, sizeof(call_addresses));
    }

    StackTrace(const StackTrace &other)
    {
        std::memcpy(call_addresses, other.call_addresses, sizeof(call_addresses));
    }

    StackTrace &operator=(const StackTrace &other)
    {
        std::memcpy(call_addresses, other.call_addresses, sizeof(call_addresses));

        return *this;
    }

    ~StackTrace() = default;
};

#endif