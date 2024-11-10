/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DATA_MUTATION_STATE_HPP
#define HYPERION_DATA_MUTATION_STATE_HPP

#include <core/threading/AtomicVar.hpp>

#include <Types.hpp>

namespace hyperion {
namespace utilities {

struct DataMutationState
{
    enum State
    {
        CLEAN = 0x0,
        DIRTY = 0x1
    };

    DataMutationState(State value = CLEAN) : state(value) {}
    DataMutationState(const DataMutationState &other) = default;
    DataMutationState &operator=(const DataMutationState &other) = default;

    DataMutationState &operator=(State value)
    {
        state = value;

        return *this;
    }

    operator bool() const { return state == CLEAN; }

    bool operator==(const DataMutationState &other) const { return state == other.state; }
    bool operator!=(const DataMutationState &other) const { return state != other.state; }

    DataMutationState &operator|=(State value)
    {
        state |= uint32(value);

        return *this;
    }

    DataMutationState &operator&=(State value)
    {
        state &= uint32(value);

        return *this;
    }

    bool IsClean() const { return state == CLEAN; }
    bool IsDirty() const { return state & DIRTY; }

private:
    uint32  state;
};

} // namespace utilities

using DataMutationState = utilities::DataMutationState;

} // namespace hyperion

#endif