/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
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

} // namespace Hyperion
