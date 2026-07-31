// strata.h - public embedding API for the Strata language (C ABI).
//
// Host applications link against the Strata shared
// library and use these functions to compile Strata source at runtime. The
// interface is pure C so it can be called from C, C++, C#, Rust, or any
// language with a foreign-function interface.
//
// Result strings are owned by the compiler and freed with strataResultFree().
#pragma once

#include <stddef.h>

#ifdef _WIN32
#if defined(STRATA_STATIC)
#define STRATA_API
#elif defined(STRATA_EXPORTS)
#define STRATA_API __declspec(dllexport)
#else
#define STRATA_API __declspec(dllimport)
#endif
#else   // !_WIN32
#define STRATA_API
#endif  // _WIN32

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct StrataCompiler StrataCompiler;

typedef enum
{
    STRATA_EMIT_LLVM_IR = 0,
    STRATA_EMIT_AST     = 1,
} StrataEmitKind;

typedef struct StrataJit StrataJit;

STRATA_API StrataJit* strataJitCompileString(StrataCompiler* c, const char* source,
                                              const char* moduleName, const char** errOut);
STRATA_API StrataJit* strataJitCompileFile(StrataCompiler* c, const char* path,
                                           const char** errOut);

STRATA_API void* strataJitGetFunction(StrataJit* jit, const char* name);

STRATA_API int strataJitAddSymbol(StrataJit* jit, const char* name, void* fn);

STRATA_API size_t strataJitGetExternSymbolCount(StrataJit* jit);
STRATA_API const char* strataJitGetExternSymbolName(StrataJit* jit, size_t index);

STRATA_API const char* strataJitDiagnostics(StrataJit* jit);
STRATA_API void strataJitDestroy(StrataJit* jit);

typedef struct
{
    int ok;
    const char* output;
    const char* diagnostics;
    unsigned error_count;
    unsigned warning_count;
} StrataResult;

STRATA_API StrataCompiler* strataCompilerCreate(void);
STRATA_API void strataCompilerDestroy(StrataCompiler* c);

STRATA_API StrataResult strataCompileString(StrataCompiler* c, const char* source,
                                            const char* moduleName, StrataEmitKind emit);
STRATA_API StrataResult strataCompileFile(StrataCompiler* c, const char* path,
                                          StrataEmitKind emit);

STRATA_API void strataResultFree(StrataResult* r);
STRATA_API void strataFree(char* s);
STRATA_API const char* strataLLVMVersion(void);

#ifdef __cplusplus
}
#endif
