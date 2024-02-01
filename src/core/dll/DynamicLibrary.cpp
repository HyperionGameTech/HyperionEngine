#include <core/dll/DynamicLibrary.hpp>
#include <util/Defines.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
#include <dlfcn.h>
#endif

namespace hyperion {

UniquePtr<DynamicLibrary> DynamicLibrary::Load(const String &path)
{
#ifdef HYP_WINDOWS
    HMODULE handle = LoadLibraryA(path.Data());

    if (handle == nullptr) {
        return nullptr;
    }

    return UniquePtr<DynamicLibrary>(new DynamicLibrary(path, reinterpret_cast<void *>(handle)));
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    void *handle = dlopen(path.Data(), RTLD_NOW);

    if (handle == nullptr) {
        return nullptr;
    }

    return UniquePtr<DynamicLibrary>(new DynamicLibrary(path, handle));
#endif
}

DynamicLibrary::DynamicLibrary(const String &path, void *handle)
    : m_path(path),
      m_handle(handle)
{
}

DynamicLibrary::~DynamicLibrary()
{
    if (!m_handle) {
        return;
    }

#ifdef HYP_WINDOWS
    FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    dlclose(m_handle);
#endif
}

void *DynamicLibrary::GetFunction(const char *name) const
{
    if (!m_handle) {
        return nullptr;
    }

#ifdef HYP_WINDOWS
    return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    return dlsym(m_handle, name);
#endif
}

} // namespace hyperion