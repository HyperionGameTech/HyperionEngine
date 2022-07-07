#ifndef HYPERION_V2_CORE_MEMORY_H
#define HYPERION_V2_CORE_MEMORY_H

#include <util/Defines.hpp>

#if HYP_MSVC
    #define HYP_MEMORY_BARRIER_COUNTER(counter, end_value) \
        do { \
            volatile uint32_t _memory_barrier_counter = 0u; \
            while (counter != end_value) { \
                ++_memory_barrier_counter; \
            } \
        } while (0)

#else
    #define HYP_MEMORY_BARRIER_COUNTER(counter, end_value) \
        do { \
            while (counter != end_value) { \
                asm("":::"memory"); \
            } \
        } while (0)

#endif

#endif
