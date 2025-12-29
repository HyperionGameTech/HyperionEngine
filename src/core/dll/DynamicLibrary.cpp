/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/dll/DynamicLibrary.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/Mutex.hpp>

#include <core/Defines.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
#include <dlfcn.h>
#endif

namespace Hyperion {

#pragma region DynamicLibraryImpl

struct DynamicLibraryImpl
{
    PlatformString path;
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

#pragma endregion DynamicLibraryImpl

#pragma region DynamicLibrary

DynamicLibrary::DynamicLibrary(const PlatformString& path)
    : m_impl(MakePimpl<DynamicLibraryImpl>())
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

const PlatformString& DynamicLibrary::GetPath() const
{
    if (!m_impl)
    {
        return PlatformString::empty;
    }

    return m_impl->path;
}

void DynamicLibrary::SetPath(const PlatformString& path)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<DynamicLibraryImpl>();
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
    HMODULE handle = reinterpret_cast<HMODULE>(LoadLibraryW(m_impl->path.Data()));

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

#pragma endregion DynamicLibrary

#pragma region DynamicLibraryCacheImpl

struct DynamicLibraryCacheImpl
{
    Mutex mutex;
    HashMap<PlatformString, UniquePtr<DynamicLibrary>> libraries;
};

#pragma endregion DynamicLibraryCacheImpl

#pragma region DynamicLibraryCache

DynamicLibraryCache& DynamicLibraryCache::GetInstance()
{
    static DynamicLibraryCache s_instance;

    return s_instance;
}

DynamicLibrary* DynamicLibraryCache::LoadLibrary(PlatformStringView path)
{
    Mutex::Guard guard(m_impl->mutex);

    auto it = m_impl->libraries.FindAs(path);

    if (it != m_impl->libraries.End())
    {
        return it->second.Get();
    }

    UniquePtr<DynamicLibrary> library = MakeUnique<DynamicLibrary>(PlatformString(path));

    if (!library->Load())
    {
        return nullptr;
    }

    auto insertResult = m_impl->libraries.Insert(path, std::move(library));

    return insertResult.first->second.Get();
}

} // namespace Hyperion
