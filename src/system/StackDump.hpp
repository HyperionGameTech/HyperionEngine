#ifndef HYPERION_STACK_DUMP_H
#define HYPERION_STACK_DUMP_H

#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <util/Defines.hpp>

#ifdef HYP_WINDOWS
#include <windows.h>
#elif defined(HYP_UNIX)
#include <execinfo.h>
#endif

namespace hyperion {

class StackDump
{
    static Array<String> CreateStackTrace(UInt depth = 20)
    {
        Array<String> stack_trace;
        stack_trace.Reserve(depth);

#ifdef HYP_WINDOWS
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, nullptr, true);

        void **stack = (void **)malloc(depth * sizeof(void *));
        const auto frames = CaptureStackBackTrace(0, depth, stack, nullptr);

        Array<StackFrameSymbolInfoHandle> symbol_info(frames);
        for (UInt32 i = 0; i < frames; ++i) {
            symbol_info[i] = static_cast<StackFrameSymbolInfoHandle>(malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char)));
            symbol_info[i]->MaxNameLen = 255;
            symbol_info[i]->SizeOfStruct = sizeof(SYMBOL_INFO);
        }

        SymSetOptions(SYMOPT_LOAD_LINES);

        SymFromAddr(process, reinterpret_cast<DWORD64>(stack[0]), nullptr, symbol_info[0]);

        for (UInt32 i = 0; i < frames; ++i) {
            SymFromAddr(process, reinterpret_cast<DWORD64>(stack[i]), nullptr, symbol_info[i]);

            String frame = String::Format("%s(%d): %s", symbol_info[i]->Name, symbol_info[i]->LineNumber, symbol_info[i]->Name);
            stack_trace.PushBack(std::move(frame));
        }

        for (UInt32 i = 0; i < frames; ++i) {
            free(symbol_info[i]);
        }

        free(stack);

        SymCleanup(process);
#elif defined(HYP_UNIX)
        void **stack = (void **)malloc(depth * sizeof(void *));
        const Int frames = backtrace(stack, depth);

        Array<String> symbols;
        symbols.Resize(frames);

        char **strings = backtrace_symbols(stack, frames);

        for (Int i = 0; i < frames; ++i) {
            symbols[i] = strings[i];
        }

        for (Int i = 0; i < frames; ++i) {
            stack_trace.PushBack(symbols[i]);
        }

        free(strings);
        free(stack);
#else
        stack_trace.PushBack("Stack trace not supported on this platform.");
#endif

        return stack_trace;
    }

public:
    StackDump(UInt depth = 20)
        : m_trace(CreateStackTrace(depth))
    {
    }

    StackDump(const StackDump &other)                   = default;
    StackDump &operator=(const StackDump &other)        = default;
    StackDump(StackDump &&other) noexcept               = default;
    StackDump &operator=(StackDump &&other) noexcept    = default;
    ~StackDump()                                        = default;

    const Array<String> &GetTrace() const
        { return m_trace; }

    String ToString() const
        { return String::Join(m_trace, "\n"); }

private:
    Array<String> m_trace;
};

} // namespace hyperion

#endif // HYPERION_STACK_DUMP_H