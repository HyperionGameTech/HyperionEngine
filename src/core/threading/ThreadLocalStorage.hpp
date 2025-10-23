/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace threading {

static constexpr SizeType TlsPoolBlockSize = 4 * 1024 * 1024; // 4 MiB

class ThreadLocalStorage : public Pool
{
public:
    ThreadLocalStorage()
        : Pool(TlsPoolBlockSize)
    {
    }

    ThreadLocalStorage(const ThreadLocalStorage& other) = delete;
    ThreadLocalStorage& operator=(const ThreadLocalStorage& other) = delete;

    ThreadLocalStorage(ThreadLocalStorage&& other) = delete;
    ThreadLocalStorage& operator=(ThreadLocalStorage&& other) = delete;

    ~ThreadLocalStorage() = default;
};

} // namespace threading

using threading::ThreadLocalStorage;

} // namespace hyperion
