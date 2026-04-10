#include "FxUtil.hpp"

#include <Core/FxDefines.hpp>

#if defined(FX_COMPILER_CLANG) || defined(FX_COMPILER_GCC)
#define FX_UTIL_DEFINE_DEMANGLE_NAME 1
#include <cxxabi.h>
#endif

int FxUtil::DemangleName(const char* mangled_name, char* buffer, size_t buffer_size)
{
#ifdef FX_UTIL_DEFINE_DEMANGLE_NAME
    int status;
    abi::__cxa_demangle(mangled_name, buffer, &buffer_size, &status);

    return status;
#else
    // Should we print a warning about this?
    strcpy(buffer, mangled_name);
    return 0;
#endif
}
