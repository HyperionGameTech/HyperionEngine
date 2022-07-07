#ifndef HYPERION_STACK_DUMP_H
#define HYPERION_STACK_DUMP_H

#include <util/Defines.hpp>

#include <vector>

#ifdef HYP_WINDOWS
#include <windows.h>

using StackFrameSymbolInfoHandle = SYMBOL_INFO *;

#else

using StackFrameSymbolInfoHandle = void *;

#endif

namespace hyperion {

struct StackFrame {
	StackFrameSymbolInfoHandle symbol_info = nullptr;
};

static std::vector<StackFrame> GetStackDump()
{
	std::vector<StackFrame> frames;

#ifdef HYP_WINDOWS
	HANDLE process = GetCurrentProcess();

	AssertThrowMsg(SymInitialize(process, NULL, true), "Could not initialize Win32 symbol handler!\n");
	uint16_t frames_count = 0;
	frames_count = CaptureStackBacktrace();

	constexpr uint32_t max_stack_frames = 128;
	void *stack[max_stack_frames];
#endif

	return frames;
}

}; // namespace hyperion

#endif // HYPERION_STACK_DUMP_H