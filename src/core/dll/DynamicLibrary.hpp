/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Pimpl.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/ObjectFwd.hpp>

namespace Hyperion {

class HYP_API DynamicLibrary
{
public:
    DynamicLibrary() = default;

    explicit DynamicLibrary(const PlatformString& path);

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    DynamicLibrary(DynamicLibrary&&) noexcept = default;
    DynamicLibrary& operator=(DynamicLibrary&&) noexcept = default;

    ~DynamicLibrary();

    const PlatformString& GetPath() const;
    void SetPath(const PlatformString& path);

    bool Load();

    UIntPtr GetFunction(const char* name) const;

private:
    Pimpl<struct DynamicLibraryImpl> m_impl;
};

class DynamicLibraryCache
{
public:
    static DynamicLibraryCache& GetInstance();

    DynamicLibrary* LoadLibrary(PlatformStringView path);

private:
    Pimpl<struct DynamicLibraryCacheImpl> m_impl;
};

} // namespace Hyperion
