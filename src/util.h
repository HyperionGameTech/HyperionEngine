#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <sstream>
#include <cctype>

namespace hyperion {

using BitFlags_t = uint64_t;

#define STR(s) #s
#define STRINGIFY(s) STR(s)

#define _print_assert(mode, cond) { const char *s = "*** " mode " assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }
#define _print_assert_msg(mode, cond, msg) { const char *s = "*** " mode " assertion failed: " #msg " ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }

#define ex_assert_msg(cond, msg) { if (!(cond)) { throw std::runtime_error("Invalid argument: " msg "\n\tCondition: " #cond " evaluated to FALSE in file " __FILE__); } }
#define ex_assert(cond) { if (!(cond)) { const char *s = "*** assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; throw std::runtime_error(s); } }
#define soft_assert(cond) { if (!(cond)) { _print_assert("soft", cond); return; } }
#define soft_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); return; } }
#define soft_assert_return(cond, value) { if (!(cond)) { _print_assert("soft", cond); return value; } }
#define soft_assert_return_msg(cond, msg, value) { if (!(cond)) { _print_assert_msg("soft", cond, msg); return (value); } }
#define soft_assert_break(cond) { if (!(cond)) { _print_assert("soft", cond); break; } }
#define soft_assert_break_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); break; } }
#define hard_assert(cond) { if (!(cond)) { _print_assert("hard", cond); exit(1); } }
#define hard_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("hard", cond, msg); exit(1); } }
#define soft_assert_continue(cond) { if (!(cond)) { _print_assert("soft", cond); continue; } }
#define soft_assert_continue_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); continue; } }

#define not_implemented hard_assert_msg(0, __func__ " not implemented")
#define function_body_not_implemented { not_implemented; }
#define unexpected_value(value) hard_assert_msg(0, STRINGIFY(value) ": unexpected value")
#define unexpected_value_msg(value, msg) hard_assert_msg(0, STRINGIFY(value) ": " #msg)

// https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
static inline uint64_t FastLog2(uint64_t value)
{
    const int tab64[64] = {
        63,  0, 58,  1, 59, 47, 53,  2,
        60, 39, 48, 27, 54, 33, 42,  3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22,  4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16,  9, 12,
        44, 24, 15,  8, 23,  7,  6,  5
    };

    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;

    return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

} // namespace hyperion

#endif
