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

    DataMutationState(State value = CLEAN)
        : m_state(value)
    {
    }

    DataMutationState(const DataMutationState& other) = default;
    DataMutationState& operator=(const DataMutationState& other) = default;

    DataMutationState& operator=(State value)
    {
        m_state = value;

        return *this;
    }

    operator bool() const
    {
        return m_state == CLEAN;
    }

    bool operator==(const DataMutationState& other) const
    {
        return m_state == other.m_state;
    }

    bool operator!=(const DataMutationState& other) const
    {
        return m_state != other.m_state;
    }

    DataMutationState& operator|=(State value)
    {
        m_state |= uint32(value);

        return *this;
    }

    DataMutationState& operator&=(State value)
    {
        m_state &= uint32(value);

        return *this;
    }

    bool IsClean() const
    {
        return m_state == CLEAN;
    }

    bool IsDirty() const
    {
        return m_state & DIRTY;
    }

private:
    uint32 m_state;
};

} // namespace utilities

using DataMutationState = utilities::DataMutationState;

} // namespace hyperion

#endif