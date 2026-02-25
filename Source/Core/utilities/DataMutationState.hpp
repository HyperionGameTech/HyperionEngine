/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/AtomicVar.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
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

} // namespace Hyperion
