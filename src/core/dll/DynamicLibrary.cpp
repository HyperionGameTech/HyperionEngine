/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/dll/DynamicLibrary.hpp>
#include <core/Defines.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
#include <dlfcn.h>

#endif

namespace hyperion {

struct DynamicLibraryImpl
{
    String path;
    UIntPtr handle;

    DynamicLibraryImpl()
        : handle(0)
    {
    }

    ~DynamicLibraryImpl()
    {
        Assert(!handle); // should have been freed by DynamicLibrary::~DynamicLibrary()
    }
};

DynamicLibrary::DynamicLibrary(const String& path)
    : m_impl(MakeRefCountedPtr<DynamicLibraryImpl>())
{
    m_impl->path = path;
}

DynamicLibrary::~DynamicLibrary()
{
    if (!m_impl)
    {
        return;
    }

#ifdef HYP_WINDOWS
    FreeLibrary(reinterpret_cast<HMODULE>(m_impl->handle));
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    dlclose(reinterpret_cast<void*>(m_impl->handle));
#endif
}

const String& DynamicLibrary::GetPath() const
{
    if (!m_impl)
    {
        return String::empty;
    }

    return m_impl->path;
}

void DynamicLibrary::SetPath(const String& path)
{
    if (!m_impl)
    {
        m_impl = MakeRefCountedPtr<DynamicLibraryImpl>();
        m_impl->path = path;

        return;
    }

    m_impl->path = path;
}

bool DynamicLibrary::Load()
{
    if (!m_impl) // disposed
    {
        return false;
    }

    if (m_impl->handle) // already loaded
    {
        return true;
    }

#ifdef HYP_WINDOWS
    WideString wpath = m_impl->path.ToWide();

    HMODULE handle = reinterpret_cast<HMODULE>(LoadLibraryW(wpath.Data()));

    if (!handle)
    {
        return false;
    }

    m_impl->handle = reinterpret_cast<UIntPtr>(handle);

    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    m_impl->handle = reinterpret_cast<UIntPtr>(dlopen(m_impl->path.Data(), RTLD_NOW));

    if (!m_impl->handle)
    {
        return false;
    }

    return true;
#else
    return false;
#endif
}

UIntPtr DynamicLibrary::GetFunction(const char* name) const
{
    if (!m_impl || !m_impl->handle)
    {
        return 0;
    }

#ifdef HYP_WINDOWS
    return reinterpret_cast<UIntPtr>(GetProcAddress(reinterpret_cast<HMODULE>(m_impl->handle), name));
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    return reinterpret_cast<UIntPtr>(dlsym(reinterpret_cast<void*>(m_impl->handle), name));
#else
    return 0;
#endif
}

} // namespace hyperion
