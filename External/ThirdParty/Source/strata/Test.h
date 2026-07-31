#pragma once

#include <stdio.h>
#include <stdlib.h>

typedef void (*TestFn)(void);

typedef struct {
    const char* name;
    TestFn fn;
} TestRegistration;

void TestRegister(const char* name, TestFn fn);
void TestFailAt(const char* expr, const char* file, int line);
void TestFailEq(const char* ea, const char* eb, long a, long b, const char* file, int line);
int TestRunAll(void);

#define STRATA_TEST(name) \
    static void name##_impl(void); \
    __attribute__((constructor)) static void name##_reg(void) { \
        TestRegister(#name, name##_impl); \
    } \
    static void name##_impl(void)

#define STRATA_CHECK(cond) \
    do { \
        if (!(cond)) TestFailAt(#cond, __FILE__, __LINE__); \
    } while (0)

#define STRATA_CHECK_EQ(a, b) \
    do { \
        long _sa = (long)(a); \
        long _sb = (long)(b); \
        if (_sa != _sb) TestFailEq(#a, #b, _sa, _sb, __FILE__, __LINE__); \
    } while (0)

#ifdef STRATA_TEST_MAIN
int main(void)
{
    return TestRunAll() == 0 ? 0 : 1;
}
#endif
