// The contents of this file are primarily AI generated

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

// Benchmark for the Strata scripting language (LLVM JIT) against a plain C++
// reference implementation. Each workload is driven by the same host harness;
// results are cross-checked against the C++ reference before being reported,
// so a silently-wrong script result fails loudly instead of being timed.

#if defined(HYP_TESTS) && defined(HYP_STRATA) && defined(HYP_STRATA_JIT)

#include <Core/Scripting/Strata/StrataMarshal.hpp>

#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Logging/Logger.hpp>

#include <strata/strata.h>

#include <cstdio>

namespace Hyperion {
namespace tests {
namespace script {

namespace {

// Number of timed repetitions per workload (a warmup run precedes them).
constexpr uint32 kReps = 5;

// Workload arguments. Sizes are chosen so a single rep takes on the order of
// tens to hundreds of milliseconds.
constexpr int32 kIntLoopArg      = 1000000;
constexpr int32 kFibArg          = 46;
constexpr int32 kMandelbrotArg   = 128;
constexpr int32 kCallOverheadArg = 100000;
constexpr int32 kArrayAccessArg  = 100000;
constexpr int32 kStringAllocArg  = 10000;

// ---------------------------------------------------------------------------
// Strata source. One module; every benchmark entry is an exported function.
// ---------------------------------------------------------------------------

const char* g_strataSource = R"(
    long int_loop(int n)
    {
        long total = 0;
        for (int i = 0; i < n; i++)
        {
            total = total + i;
        }
        return total;
    }

    long fib(int n)
    {
        long a = 0;
        long b = 1;
        for (int i = 0; i < n; i++)
        {
            long next = a + b;
            a = b;
            b = next;
        }
        return a;
    }

    long mandelbrot(int res)
    {
        long count = 0;
        for (int y = 0; y < res; y++)
        {
            double ci = -1.0 + (2.0 * (double)y) / (double)res;
            for (int x = 0; x < res; x++)
            {
                double cr = -2.0 + (2.5 * (double)x) / (double)res;
                double zr = 0.0;
                double zi = 0.0;
                int iter = 0;
                while (iter < 64)
                {
                    double zr2 = zr * zr;
                    double zi2 = zi * zi;
                    if (zr2 + zi2 > 4.0) { break; }
                    zi = 2.0 * zr * zi + ci;
                    zr = zr2 - zi2 + cr;
                    iter++;
                }
                count = count + iter;
            }
        }
        return count;
    }

    int add_one(int x)
    {
        return x + 1;
    }

    long call_overhead(int n)
    {
        int total = 0;
        for (int i = 0; i < n; i++)
        {
            total = add_one(total);
        }
        return total;
    }

    long array_access(int n)
    {
        int[] a = {};
        array_resize(a, n);
        for (int i = 0; i < n; i++)
        {
            a[i] = i;
        }
        long total = 0;
        for (ulong i = 0; i < a.length; i++)
        {
            total = total + a[i];
        }
        return total;
    }

    long string_alloc(int n)
    {
        string[] arr = {};
        for (int i = 0; i < n; i++)
        {
            array_push(arr, "hello world");
        }
        long total = 0;
        for (ulong i = 0; i < arr.length; i++)
        {
            total = total + (long)arr[i].length;
        }
        return total;
    }
)";

// ---------------------------------------------------------------------------
// Reference implementations (host C++). "hello world" is 11 characters.
// ---------------------------------------------------------------------------

int64 RefIntLoop(int32 n)
{
    int64 total = 0;

    for (int32 i = 0; i < n; ++i)
    {
        total += i;
    }

    return total;
}

int64 RefFib(int32 n)
{
    int64 a = 0;
    int64 b = 1;

    for (int32 i = 0; i < n; ++i)
    {
        const int64 next = a + b;
        a = b;
        b = next;
    }

    return a;
}

int64 RefMandelbrot(int32 res)
{
    int64 count = 0;

    for (int32 y = 0; y < res; ++y)
    {
        const double ci = -1.0 + (2.0 * double(y)) / double(res);

        for (int32 x = 0; x < res; ++x)
        {
            const double cr = -2.0 + (2.5 * double(x)) / double(res);
            double zr = 0.0;
            double zi = 0.0;
            int32 iter = 0;

            while (iter < 64)
            {
                const double zr2 = zr * zr;
                const double zi2 = zi * zi;

                if (zr2 + zi2 > 4.0)
                {
                    break;
                }

                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                ++iter;
            }

            count += iter;
        }
    }

    return count;
}

int64 RefCallOverhead(int32 n)
{
    return n;
}

int64 RefArrayAccess(int32 n)
{
    int64 total = 0;

    for (int32 i = 0; i < n; ++i)
    {
        total += i;
    }

    return total;
}

int64 RefStringAlloc(int32 n)
{
    return int64(n) * 11;
}

// ---------------------------------------------------------------------------
// Workload table
// ---------------------------------------------------------------------------

struct Workload
{
    const char* name;
    int32 arg;
    int64 (*hostFn)(int32);
    int64 (*strataFn)(int32);
    const char* strataFnName;
};

constexpr uint32 kWorkloadCount = 6;

Workload s_workloads[kWorkloadCount] = {
    { "int_loop",      kIntLoopArg,      &RefIntLoop,      nullptr, "int_loop" },
    { "fib",           kFibArg,          &RefFib,          nullptr, "fib" },
    { "mandelbrot",    kMandelbrotArg,   &RefMandelbrot,   nullptr, "mandelbrot" },
    { "call_overhead", kCallOverheadArg, &RefCallOverhead, nullptr, "call_overhead" },
    { "array_access",  kArrayAccessArg,  &RefArrayAccess,  nullptr, "array_access" },
    { "string_alloc",  kStringAllocArg,  &RefStringAlloc,  nullptr, "string_alloc" }
};

template <class Fn>
static double RunTimed(Fn&& fn, int64& outResult)
{
    outResult = fn(); // warmup, untimed

    double bestMs = 1e300;

    for (uint32 i = 0; i < kReps; ++i)
    {
        const uint64 start = PerformanceClock::Now();
        outResult = fn();
        const double ms = PerformanceClock::TimeSince(start);

        if (ms < bestMs)
        {
            bestMs = ms;
        }
    }

    return bestMs;
}

} // anonymous namespace

HYP_EXPORT void RunScriptBenchmark()
{
    StrataCompiler* compiler = nullptr;
    StrataJit* jit = nullptr;

    // ---- Strata: JIT compile the module and resolve entry points -----------
    compiler = strataCompilerCreate();

    if (compiler == nullptr)
    {
        HYP_LOG(Engine, Error, "ScriptBenchmark: failed to create Strata compiler");

        return;
    }

    strataJitSetAllocFreeFunctions(compiler, (void*)&Strata::Alloc, (void*)&Strata::Free);

    {
        const char* err = nullptr;
        jit = strataJitCompileString(compiler, g_strataSource, "benchmark", &err);

        if (jit == nullptr)
        {
            HYP_LOG(Engine, Error, "ScriptBenchmark: Strata JIT compile failed: {}",
                err ? err : "(no message)");

            if (err != nullptr)
            {
                strataFree(const_cast<char*>(err));
            }
        }
        else
        {
            for (uint32 i = 0; i < kWorkloadCount; ++i)
            {
                s_workloads[i].strataFn = reinterpret_cast<int64 (*)(int32)>(strataJitGetFunction(jit, s_workloads[i].strataFnName));
            }
        }
    }

    // ---- Run ----------------------------------------------------------------
    HYP_LOG(Engine, Info, "========== Strata Benchmark ==========");
    HYP_LOG(Engine, Info, "{} rep(s) per workload, best time reported", kReps);

    char line[256];

    snprintf(line, sizeof(line), "%-15s %10s %10s %9s",
        "workload", "C++(ms)", "Strata(ms)", "Strata/C++");
    HYP_LOG(Engine, Info, "{}", static_cast<const char*>(line));

    for (uint32 i = 0; i < kWorkloadCount; ++i)
    {
        Workload& w = s_workloads[i];

        // C++ baseline (the reference).
        int64 cppResult = 0;
        const double cppMs = RunTimed([&] { return w.hostFn(w.arg); }, cppResult);

        // Strata.
        double strataMs = -1.0;
        int64 strataResult = 0;

        if (w.strataFn != nullptr)
        {
            strataMs = RunTimed([&] { return w.strataFn(w.arg); }, strataResult);
        }

        const bool strataOk = strataMs >= 0.0 && strataResult == cppResult;

        // ratio is meaningless when the baseline is below timer resolution
        char strataRatio[16];

        if (cppMs > 0.0)
        {
            snprintf(strataRatio, sizeof(strataRatio), "%7.2fx", strataMs / cppMs);
        }
        else
        {
            snprintf(strataRatio, sizeof(strataRatio), "%7s", "n/a");
        }

        snprintf(line, sizeof(line), "%-15s %10.3f %10.3f %8s",
            w.name,
            cppMs,
            strataMs >= 0.0 ? strataMs : 0.0,
            strataRatio);
        HYP_LOG(Engine, Info, "{}", static_cast<const char*>(line));

        HYP_LOG(Engine, Info, "  [{}] Strata: {} (expected {})",
            strataOk ? "PASS" : "FAIL", strataResult, cppResult);
    }

    HYP_LOG(Engine, Info, "======================================");

    // ---- Cleanup ------------------------------------------------------------
    if (jit != nullptr)
    {
        strataJitDestroy(jit);
    }

    if (compiler != nullptr)
    {
        strataCompilerDestroy(compiler);
    }
}

} // namespace script
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS && HYP_STRATA && HYP_STRATA_JIT
