/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_TRAITS_H
#define HYPERION_V2_LIB_TRAITS_H

#include <type_traits>

namespace hyperion {

template <bool DefaultConstructible, bool Copyable, bool Moveable, class Type>
struct ConstructAssignmentTraits { };

template <class Type>
struct ConstructAssignmentTraits<true, false, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<true, false, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};


template <class Type>
struct ConstructAssignmentTraits<false, false, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, false, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

} // namespace hyperion

#endif