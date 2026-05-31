/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>
#include <Core/containers/FixedArray.hpp>
#include <Core/memory/Pimpl.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {
namespace debug {

/*! \brief Lightweight stack trace that stores only raw frame pointers.
 *  Trivially destructible and suitable for embedding in POD types.
 *  Call ToStackDump() to convert to full string representation. */
struct CORE_API RawStackTrace
{
    static constexpr uint32 MaxFrames = 20;

    void* frames[MaxFrames];
    uint32 numFrames;

    RawStackTrace()
        : RawStackTrace(MaxFrames)
    {
    }

    explicit RawStackTrace(uint32 depth, uint32 offset = 0);

    RawStackTrace(const RawStackTrace&) = default;
    RawStackTrace& operator=(const RawStackTrace&) = default;

    RawStackTrace(RawStackTrace&&) noexcept = default;
    RawStackTrace& operator=(RawStackTrace&&) noexcept = default;

    ~RawStackTrace() = default;

    /*! \brief Convert to a StackDump with string representations of frames.
     *   \note The returned StackDump must be deleted by the caller. */
    class StackDump* ToStackDump() const;
};

static_assert(std::is_trivially_destructible_v<RawStackTrace>, "RawStackTrace must be trivially destructible");

class CORE_API StackDump
{
    struct Impl;

public:
    StackDump(uint32 depth = 20, uint32 offset = 0);
    StackDump(const RawStackTrace& rawTrace);

    StackDump(const StackDump& other);
    StackDump& operator=(const StackDump& other);
    StackDump(StackDump&& other) noexcept = default;
    StackDump& operator=(StackDump&& other) noexcept = default;
    ~StackDump();

    const Array<String>& GetTrace() const;

    String ToString() const;

private:
    Pimpl<Impl> m_impl;
};

} // namespace debug

using debug::RawStackTrace;
using debug::StackDump;

} // namespace Hyperion
