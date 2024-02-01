#ifndef HYPERION_CORE_DLL_DYNAMIC_LIBRARY_HPP
#define HYPERION_CORE_DLL_DYNAMIC_LIBRARY_HPP

#include <core/lib/UniquePtr.hpp>
#include <core/lib/String.hpp>

namespace hyperion {

class DynamicLibrary
{
public:
    static UniquePtr<DynamicLibrary> Load(const String &path);

    DynamicLibrary(const String &path, void *handle);
    DynamicLibrary(const DynamicLibrary &)                  = delete;
    DynamicLibrary &operator=(const DynamicLibrary &)       = delete;
    DynamicLibrary(DynamicLibrary &&) noexcept              = delete;
    DynamicLibrary &operator=(DynamicLibrary &&) noexcept   = delete;
    ~DynamicLibrary();

    const String &GetPath() const
        { return m_path; }

    void *GetFunction(const char *name) const;

private:
    String  m_path;
    void    *m_handle;
};

} // namespace hyperion

#endif