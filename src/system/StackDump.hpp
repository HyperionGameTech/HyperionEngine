/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_STACK_DUMP_H
#define HYPERION_STACK_DUMP_H

#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <core/Defines.hpp>

namespace hyperion {

class StackDump
{
public:
    HYP_API StackDump(uint depth = 20);

    StackDump(const StackDump &other)                   = default;
    StackDump &operator=(const StackDump &other)        = default;
    StackDump(StackDump &&other) noexcept               = default;
    StackDump &operator=(StackDump &&other) noexcept    = default;
    ~StackDump()                                        = default;

    const Array<String> &GetTrace() const
        { return m_trace; }

    String ToString() const
        { return String::Join(m_trace, "\n"); }

private:
    Array<String>   m_trace;
};

} // namespace hyperion

#endif // HYPERION_STACK_DUMP_H