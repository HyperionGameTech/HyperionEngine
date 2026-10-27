/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Defines.hpp>

#ifdef HYP_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#elif defined(HYP_UNIX)

#include <dlfcn.h>
#include <climits>
#include <cstdlib>
#include <cstring>

#if defined(HYP_MACOS)

#include <mach-o/dyld.h>
#include <mach-o/loader.h>

#elif HYP_LINUX

#include <link.h>

#endif

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
    if (!m_impl || !m_impl->handle)
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

bool DynamicLibrary::GetImageRange(UIntPtr& outBegin, UIntPtr& outEnd) const
{
    if (!m_impl || !m_impl->handle)
    {
        return false;
    }

#if HYP_WINDOWS
    const UIntPtr base = reinterpret_cast<UIntPtr>(m_impl->handle);

    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

    outBegin = base;
    outEnd = base + ntHeaders->OptionalHeader.SizeOfImage;

    return true;
#elif HYP_MACOS
    char libraryPath[PATH_MAX];

    if (!realpath(m_impl->path.Data(), libraryPath))
    {
        return false;
    }

    for (uint32_t imageIndex = 0; imageIndex < _dyld_image_count(); imageIndex++)
    {
        char imagePath[PATH_MAX];

        if (!realpath(_dyld_get_image_name(imageIndex), imagePath) || std::strcmp(imagePath, libraryPath) != 0)
        {
            continue;
        }

        const mach_header* header = _dyld_get_image_header(imageIndex);
        const intptr_t slide = _dyld_get_image_vmaddr_slide(imageIndex);

        if (header == nullptr || header->magic != MH_MAGIC_64)
        {
            return false;
        }

        UIntPtr begin = ~UIntPtr(0);
        UIntPtr end = 0;

        const load_command* command = reinterpret_cast<const load_command*>(reinterpret_cast<const mach_header_64*>(header) + 1);

        for (uint32_t commandIndex = 0; commandIndex < header->ncmds; commandIndex++)
        {
            if (command->cmd == LC_SEGMENT_64)
            {
                const segment_command_64* segment = reinterpret_cast<const segment_command_64*>(command);

                if (segment->vmsize != 0 && std::strcmp(segment->segname, SEG_PAGEZERO) != 0)
                {
                    begin = MathUtil::Min(begin, UIntPtr(segment->vmaddr + slide));
                    end = MathUtil::Max(end, UIntPtr(segment->vmaddr + segment->vmsize + slide));
                }
            }

            command = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(command) + command->cmdsize);
        }

        outBegin = begin;
        outEnd = end;

        return end > begin;
    }

    return false;
#elif HYP_LINUX
    link_map* linkMap = nullptr;

    if (dlinfo(m_impl->handle, RTLD_DI_LINKMAP, &linkMap) != 0 || linkMap == nullptr)
    {
        return false;
    }

    struct ImageSearch
    {
        const link_map* linkMap;
        UIntPtr begin;
        UIntPtr end;
    };

    ImageSearch search { linkMap, ~UIntPtr(0), 0 };

    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* data) -> int
        {
            ImageSearch& search = *static_cast<ImageSearch*>(data);

            if (info->dlpi_addr != search.linkMap->l_addr || info->dlpi_name == nullptr || std::strcmp(info->dlpi_name, search.linkMap->l_name) != 0)
            {
                return 0;
            }

            for (int headerIndex = 0; headerIndex < info->dlpi_phnum; headerIndex++)
            {
                const ElfW(Phdr)& programHeader = info->dlpi_phdr[headerIndex];

                if (programHeader.p_type == PT_LOAD)
                {
                    search.begin = MathUtil::Min(search.begin, UIntPtr(info->dlpi_addr + programHeader.p_vaddr));
                    search.end = MathUtil::Max(search.end, UIntPtr(info->dlpi_addr + programHeader.p_vaddr + programHeader.p_memsz));
                }
            }

            return 1;
        },
        &search);

    outBegin = search.begin;
    outEnd = search.end;

    return search.end > search.begin;
#else
    return false;
#endif
}

PlatformString DynamicLibrary::GetPathOfAddress(const void* address)
{
#ifdef HYP_WINDOWS
    HMODULE module = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module))
    {
        return PlatformString();
    }

    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);

    if (length == 0 || length >= MAX_PATH)
    {
        return PlatformString();
    }

    return PlatformString(buffer, buffer + length);
#elif defined(HYP_UNIX)
    Dl_info info;

    if (dladdr(address, &info) == 0 || info.dli_fname == nullptr)
    {
        return PlatformString();
    }

    return PlatformString(info.dli_fname);
#else
    return PlatformString();
#endif
}

#pragma endregion DynamicLibrary

#pragma region DynamicLibraryCacheImpl

struct DynamicLibraryCacheImpl
{
    Mutex mutex;
    Map<PlatformString, UniquePtr<DynamicLibrary>> libraries;
};

#pragma endregion DynamicLibraryCacheImpl

#pragma region DynamicLibraryCache

DynamicLibraryCache& DynamicLibraryCache::GetInstance()
{
    static DynamicLibraryCache s_instance;

    return s_instance;
}

DynamicLibraryCache::DynamicLibraryCache()
    : m_impl(MakePimpl<DynamicLibraryCacheImpl>())
{
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

bool DynamicLibraryCache::UnloadLibrary(PlatformStringView path)
{
    UniquePtr<DynamicLibrary> library;

    {
        Mutex::Guard guard(m_impl->mutex);

        auto it = m_impl->libraries.FindAs(path);

        if (it == m_impl->libraries.End())
        {
            return false;
        }

        library = std::move(it->second);
        m_impl->libraries.Erase(it);
    }

    library.Reset();

    return true;
}

} // namespace Hyperion
