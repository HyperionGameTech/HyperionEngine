/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/String.hpp>

namespace hyperion {

HYP_STRUCT(Size = 16)
class HYP_API DynamicLibrary
{
public:
    DynamicLibrary() = default;

    explicit DynamicLibrary(const String& path);

    DynamicLibrary(const DynamicLibrary&) = default;
    DynamicLibrary& operator=(const DynamicLibrary&) = default;
    DynamicLibrary(DynamicLibrary&&) noexcept = default;
    DynamicLibrary& operator=(DynamicLibrary&&) noexcept = default;
    ~DynamicLibrary();

    HYP_METHOD()
    const String& GetPath() const;

    HYP_METHOD()
    void SetPath(const String& path);

    HYP_METHOD()
    bool Load();

    HYP_METHOD()
    UIntPtr GetFunction(const char* name) const;

private:
    RC<struct DynamicLibraryImpl> m_impl;
};

} // namespace hyperion
