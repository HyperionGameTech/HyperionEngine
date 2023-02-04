#ifndef HYPERION_V2_SHADER_DATA_STATE_HPP
#define HYPERION_V2_SHADER_DATA_STATE_HPP

#include <Types.hpp>

namespace hyperion::v2 {

struct ShaderDataState
{
    enum State
    {
        CLEAN = 0x0,
        DIRTY = 0x1
    };

    ShaderDataState(State value = CLEAN) : state(value) {}
    ShaderDataState(const ShaderDataState &other) = default;
    ShaderDataState &operator=(const ShaderDataState &other) = default;

    ShaderDataState &operator=(State value)
    {
        state = value;

        return *this;
    }

    operator bool() const { return state == CLEAN; }

    bool operator==(const ShaderDataState &other) const { return state == other.state; }
    bool operator!=(const ShaderDataState &other) const { return state != other.state; }

    ShaderDataState &operator|=(State value)
    {
        state |= UInt32(value);

        return *this;
    }

    ShaderDataState &operator&=(State value)
    {
        state &= UInt32(value);

        return *this;
    }

    bool IsClean() const { return state == CLEAN; }
    bool IsDirty() const { return state & DIRTY; }

private:
    UInt32 state;
};

} // namespace hyperion::v2

#endif