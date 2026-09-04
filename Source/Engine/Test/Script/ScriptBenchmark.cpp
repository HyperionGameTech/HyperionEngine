// The contents of this file are primarily AI generated

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

// Benchmark comparing Strata (LLVM JIT) vs HypScript (bytecode VM) execution
// performance. Each workload is written once per language and driven by the
// same host harness, which also times a plain C++ reference implementation as
// a baseline. Results are cross-checked against the C++ reference before being
// reported, so a silently-wrong script result fails loudly instead of being
// timed.
//
// NOTE: HypScript has no direct self-recursion support (self-referential
// function declarations), so `fib` is written ITERATIVELY in both languages to
// keep the comparison apples-to-apples.

/* 
    Engine [Info]: ========== Script Language Benchmark ==========
    Engine [Info]: 5 rep(s) per workload, best time reported
    Engine [Info]: workload           C++(ms) Strata(ms) HypScript(ms) Strata/C++ Hyp/Strata
    Engine [Info]: int_loop             0.000      0.364     302.469      n/a  830.05x
    Engine [Info]:   [PASS] Strata: 499999500000 (expected 499999500000)
    Engine [Info]:   [PASS] HypScript: 499999500000 (expected 499999500000)
    Engine [Info]: fib                  0.000      0.000       0.017      n/a      n/a
    Engine [Info]:   [PASS] Strata: 1836311903 (expected 1836311903)
    Engine [Info]:   [PASS] HypScript: 1836311903 (expected 1836311903)
    Engine [Info]: mandelbrot           0.745      1.923     335.705    2.58x  174.56x
    Engine [Info]:   [PASS] Strata: 402802 (expected 402802)
    Engine [Info]:   [PASS] HypScript: 402802 (expected 402802)
    Engine [Info]: call_overhead        0.000      0.334      33.832      n/a  101.39x
    Engine [Info]:   [PASS] Strata: 100000 (expected 100000)
    Engine [Info]:   [PASS] HypScript: 100000 (expected 100000)
    Engine [Info]: array_access         0.000      0.124      62.635      n/a  503.09x
    Engine [Info]:   [PASS] Strata: 4999950000 (expected 4999950000)
    Engine [Info]:   [PASS] HypScript: 4999950000 (expected 4999950000)
    Engine [Info]: string_alloc         0.000     21.790      11.013 217904.00x    0.51x
    Engine [Info]:   [PASS] Strata: 110000 (expected 110000)
    Engine [Info]:   [PASS] HypScript: 110000 (expected 110000)
    Engine [Info]: ==============================================


    OUCH HypScript!
*/

#if defined(HYP_TESTS) && defined(HYP_SCRIPT) && defined(HYP_STRATA) && defined(HYP_STRATA_JIT)

#include <Lang/HypScript.hpp>
#include <Lang/SourceFile.hpp>
#include <Lang/Compiler/ErrorList.hpp>

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

// Workload arguments. Sizes are chosen so the slower language (HypScript VM)
// takes on the order of tens to hundreds of milliseconds per rep.
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
// HypScript source. One module; every benchmark entry is an exported function.
// ---------------------------------------------------------------------------

const char* g_hypscriptSource = R"(
    export int_loop := (n : int) -> int64
        total : int64 = 0
        for (i := 0; i < n; i++)
            total += (i as int64)
        end
        return total
    end

    export fib := (n : int) -> int64
        a : int64 = 0
        b : int64 = 1
        for (i := 0; i < n; i++)
            next := a + b
            a = b
            b = next
        end
        return a
    end

    export mandelbrot := (res : int) -> int
        count := 0
        for (y := 0; y < res; y++)
            ci := -1.0 + (2.0 * (y as float)) / (res as float)
            for (x := 0; x < res; x++)
                cr := -2.0 + (2.5 * (x as float)) / (res as float)
                zr := 0.0
                zi := 0.0
                iter := 0
                while (iter < 64)
                    zr2 := zr * zr
                    zi2 := zi * zi
                    if (zr2 + zi2 > 4.0)
                        break
                    end
                    zi = 2.0 * zr * zi + ci
                    zr = zr2 - zi2 + cr
                    iter++
                end
                count += iter
            end
        end
        return count
    end

    export add_one := (x : int) -> int
        return x + 1
    end

    export call_overhead := (n : int) -> int
        total := 0
        for (i := 0; i < n; i++)
            total = add_one(total)
        end
        return total
    end

    export array_access := (n : int) -> int64
        a : Array<int> = []
        for (i := 0; i < n; i++)
            a.PushBack(i)
        end
        total : int64 = 0
        for (v in a)
            total += (v as int64)
        end
        return total
    end

    export string_alloc := (n : int) -> int
        arr : Array<string> = []
        for (i := 0; i < n; i++)
            arr.PushBack("hello world")
        end
        total := 0
        for (s in arr)
            total += (s.Length() as int)
        end
        return total
    end
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
    const char* hypscriptFnName;
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
    HypScript::Initialize();

    StrataCompiler* compiler = nullptr;
    StrataJit* jit = nullptr;
    ScriptInstance* instance = nullptr;

    BoxedValue hypscriptFns[kWorkloadCount];

    // ---- Strata: JIT compile the module and resolve entry points -----------
    compiler = strataCompilerCreate();

    if (compiler == nullptr)
    {
        HYP_LOG(Engine, Error, "ScriptBenchmark: failed to create Strata compiler");

        HypScript::Shutdown();
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
                s_workloads[i].strataFn = reinterpret_cast<int64 (*)(int32)>(strataJitGetFunction(jit, s_workloads[i].hypscriptFnName));
            }
        }
    }

    // ---- HypScript: compile the module and run it --------------------------
    {
        const String source(g_hypscriptSource);

        SourceFile sourceFile(FilePath("benchmark.hyp"), source.Size());
        sourceFile.ReadIntoBuffer(reinterpret_cast<const ubyte*>(source.Data()), source.Size());

        ErrorList errors;
        instance = HypScript::Compile(sourceFile, errors);

        if (instance == nullptr || errors.HasFatalErrors())
        {
            HYP_LOG(Engine, Error, "ScriptBenchmark: HypScript compile failed");
            instance = nullptr;
        }
        else
        {
            HypScript::Run(instance);

            for (uint32 i = 0; i < kWorkloadCount; ++i)
            {
                if (!HypScript::GetFunctionHandle(instance, s_workloads[i].hypscriptFnName, hypscriptFns[i]))
                {
                    HYP_LOG(Engine, Error, "ScriptBenchmark: HypScript function '{}' not found",
                        s_workloads[i].hypscriptFnName);
                }
            }
        }
    }

    // ---- Run ----------------------------------------------------------------
    HYP_LOG(Engine, Info, "========== Script Language Benchmark ==========");
    HYP_LOG(Engine, Info, "{} rep(s) per workload, best time reported", kReps);

    char line[256];

    snprintf(line, sizeof(line), "%-15s %10s %10s %11s %9s %9s",
        "workload", "C++(ms)", "Strata(ms)", "HypScript(ms)", "Strata/C++", "Hyp/Strata");
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

        // HypScript.
        double hypMs = -1.0;
        int64 hypResult = 0;

        if (instance != nullptr && hypscriptFns[i].IsValid())
        {
            const BoxedValue& fn = hypscriptFns[i];

            hypMs = RunTimed([&] {
                BoxedValue r = HypScript::CallFunction(instance, fn, int64(w.arg));
                return r.Get<int64>();
            }, hypResult);
        }

        const bool strataOk = strataMs >= 0.0 && strataResult == cppResult;
        const bool hypOk = hypMs >= 0.0 && hypResult == cppResult;

        // ratios are meaningless when the denominator is below timer resolution
        char strataRatio[16];
        char hypRatio[16];

        if (cppMs > 0.0)
        {
            snprintf(strataRatio, sizeof(strataRatio), "%7.2fx", strataMs / cppMs);
        }
        else
        {
            snprintf(strataRatio, sizeof(strataRatio), "%7s", "n/a");
        }

        if (strataMs > 0.0)
        {
            snprintf(hypRatio, sizeof(hypRatio), "%7.2fx", hypMs / strataMs);
        }
        else
        {
            snprintf(hypRatio, sizeof(hypRatio), "%7s", "n/a");
        }

        snprintf(line, sizeof(line), "%-15s %10.3f %10.3f %11.3f %8s %8s",
            w.name,
            cppMs,
            strataMs >= 0.0 ? strataMs : 0.0,
            hypMs >= 0.0 ? hypMs : 0.0,
            strataRatio,
            hypRatio);
        HYP_LOG(Engine, Info, "{}", static_cast<const char*>(line));

        HYP_LOG(Engine, Info, "  [{}] Strata: {} (expected {})",
            strataOk ? "PASS" : "FAIL", strataResult, cppResult);
        HYP_LOG(Engine, Info, "  [{}] HypScript: {} (expected {})",
            hypOk ? "PASS" : "FAIL", hypResult, cppResult);
    }

    HYP_LOG(Engine, Info, "==============================================");

    // ---- Cleanup ------------------------------------------------------------
    if (instance != nullptr)
    {
        HypScript::DestroyScript(instance);
    }

    if (jit != nullptr)
    {
        strataJitDestroy(jit);
    }

    if (compiler != nullptr)
    {
        strataCompilerDestroy(compiler);
    }

    HypScript::Shutdown();
}

} // namespace script
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS && HYP_SCRIPT && HYP_STRATA && HYP_STRATA_JIT