/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/StackDump.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#elif defined(HYP_UNIX)
#include <execinfo.h>
#endif

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(StackTrace, Core);

namespace debug {

static Array<String> CreatePlatformStackTrace(uint32 depth, uint32 offset)
{
    Array<String> stackTrace;
    stackTrace.Reserve(depth);

#ifdef HYP_WINDOWS
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, true);

    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    STACKFRAME64 stackFrame = {};
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrStack.Offset = context.Rsp;

#ifdef HYP_ARM
    DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#endif
    
    uint32 index = 0;

    while (index < depth && StackWalk64(machineType, process, GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
    {
        DWORD64 address = stackFrame.AddrPC.Offset;
        if (address == 0)
        {
            break;
        }

        DWORD64 displacementSym = 0;
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        if (SymFromAddr(process, address, &displacementSym, symbol))
        {
            char line[2000];
            sprintf_s(line, "%s - 0x%0llX", symbol->Name, symbol->Address);
            stackTrace.PushBack(line);
        }
        else
        {
            stackTrace.PushBack("(unknown)");
        }

        index++;
    }

    SymCleanup(process);
#elif defined(HYP_UNIX)
    offset += 2;

    void** stack = (void**)malloc((depth + offset) * sizeof(void*));
    const int frames = backtrace(stack, depth + offset);

    if (frames - int(offset) > 0)
    {
        Array<String> symbols;
        symbols.Resize(frames - offset);

        char** strings = backtrace_symbols(stack, frames);

        for (int i = offset; i < frames; ++i)
        {
            symbols[i - offset] = strings[i];
        }

        for (int i = 0; i < frames - offset; ++i)
        {
            stackTrace.PushBack(symbols[i]);
        }

        free(strings);
    }

    free(stack);
#else
    stackTrace.PushBack("Stack trace not supported on this platform.");
#endif

    return stackTrace;
}

StackDump::StackDump(uint32 depth, uint32 offset)
    : m_trace(CreatePlatformStackTrace(depth, offset))
{
}

// Implementation of global LogStackTrace() function from Defines.hpp
HYP_API void LogStackTrace(int depth)
{
    HYP_LOG(StackTrace, Debug, "Stack trace:\n\n{}", StackDump(depth, 1).ToString());
}

} // namespace debug
} // namespace hyperion
