/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/dll/DynamicLibrary.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/String.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/Defines.hpp>

#if HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif HYP_UNIX
#include <dlfcn.h>
// so HMODULE can be used as a type for the handle on both Windows and Linux/MacOS
typedef void* HMODULE;
#else
#error "Unsupported platform for DynamicLibrary"
#endif

namespace Hyperion {

#pragma region DynamicLibraryImpl

struct DynamicLibraryImpl
{
    PlatformString path;
    HMODULE handle;

    DynamicLibraryImpl()
        : handle(NULL)
    {
    }

    ~DynamicLibraryImpl()
    {
        Assert(handle == NULL); // should have been freed by DynamicLibrary::~DynamicLibrary()
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
    Close();
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

bool DynamicLibrary::Open()
{
    if (!m_impl) // disposed
    {
        return false;
    }

    if (m_impl->handle) // already loaded
    {
        return true;
    }

#if HYP_WINDOWS
    m_impl->handle = LoadLibraryW(m_impl->path.Data());
#elif HYP_UNIX
    m_impl->handle = dlopen(m_impl->path.Data(), RTLD_NOW);
#else
    return false;
#endif

    return m_impl->handle != NULL;
}

void DynamicLibrary::Close()
{
    if (!m_impl)
    {
        return;
    }

#if HYP_WINDOWS
    FreeLibrary(m_impl->handle);
#elif HYP_UNIX
    dlclose(m_impl->handle);
#endif

    m_impl->handle = NULL;
}

UIntPtr DynamicLibrary::GetFunction(const char* name) const
{
    if (!m_impl || !m_impl->handle)
    {
        return 0;
    }

#if HYP_WINDOWS
    return reinterpret_cast<UIntPtr>(GetProcAddress(reinterpret_cast<HMODULE>(m_impl->handle), name));
#elif HYP_UNIX
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

    if (!library->Open())
    {
        return nullptr;
    }

    auto insertResult = m_impl->libraries.Insert(path, std::move(library));

    return insertResult.first->second.Get();
}

} // namespace Hyperion
