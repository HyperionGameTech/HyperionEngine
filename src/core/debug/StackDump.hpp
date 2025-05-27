/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STACK_DUMP_HPP
#define HYPERION_STACK_DUMP_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace debug {

class HYP_API StackDump
{
public:
    StackDump(uint32 depth = 20, uint32 offset = 0);

    StackDump(const StackDump& other) = default;
    StackDump& operator=(const StackDump& other) = default;
    StackDump(StackDump&& other) noexcept = default;
    StackDump& operator=(StackDump&& other) noexcept = default;
    ~StackDump() = default;

    HYP_FORCE_INLINE const Array<String>& GetTrace() const
    {
        return m_trace;
    }

    HYP_FORCE_INLINE String ToString() const
    {
        return String::Join(m_trace, "\n");
    }

private:
    Array<String> m_trace;
};

} // namespace debug

using debug::StackDump;

} // namespace hyperion

#endif