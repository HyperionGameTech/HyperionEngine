/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/StackDump.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#elif defined(HYP_UNIX)
#include <execinfo.h>
#endif

namespace hyperion {

static Array<String> CreatePlatformStackTrace(uint depth)
{
    Array<String> stack_trace;
    stack_trace.Reserve(depth);

#ifdef HYP_WINDOWS
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, true);

    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    STACKFRAME64 stack_frame = {};
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;
    stack_frame.AddrPC.Offset = context.Rip;
    stack_frame.AddrFrame.Offset = context.Rbp;
    stack_frame.AddrStack.Offset = context.Rsp;

    DWORD machine_type = IMAGE_FILE_MACHINE_AMD64; // Use IMAGE_FILE_MACHINE_I386 for x86
    uint index = 0;
    
    while (index < depth && StackWalk64(
        machine_type,
        process,
        GetCurrentThread(),
        &stack_frame,
        &context,
        NULL,
        SymFunctionTableAccess64,
        SymGetModuleBase64,
        NULL
    ))
    {
        DWORD64 address = stack_frame.AddrPC.Offset;
        if (address == 0) {
            break;
        }

        DWORD64 displacementSym = 0;
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        if (SymFromAddr(process, address, &displacementSym, symbol)) {
            char line[MAX_PATH];
            sprintf_s(line, "%s - 0x%0llX", symbol->Name, symbol->Address);
            stack_trace.PushBack(line);
        } else {
            stack_trace.PushBack("(unknown)");
        }

        index++;
    }

    SymCleanup(process);
#elif defined(HYP_UNIX)
    void **stack = (void **)malloc(depth * sizeof(void *));
    const int frames = backtrace(stack, depth);

    Array<String> symbols;
    symbols.Resize(frames);

    char **strings = backtrace_symbols(stack, frames);

    for (int i = 0; i < frames; ++i) {
        symbols[i] = strings[i];
    }

    for (int i = 0; i < frames; ++i) {
        stack_trace.PushBack(symbols[i]);
    }

    free(strings);
    free(stack);
#else
    stack_trace.PushBack("Stack trace not supported on this platform.");
#endif

    return stack_trace;
}

HYP_API StackDump::StackDump(uint depth)
    : m_trace(CreatePlatformStackTrace(depth))
{
}

} // namespace hyperion