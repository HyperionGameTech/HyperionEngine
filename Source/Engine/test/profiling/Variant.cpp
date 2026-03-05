/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#ifdef HYP_TESTS

#include <Core/profiling/Profile.hpp>

#include <Core/utilities/Variant.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <cstdio>
#include <string>
#include <variant>

namespace Hyperion {
namespace tests {
namespace profiling {

namespace {

constexpr size_t NumVariantOps = 5000;

volatile uint64 g_variantSink = 0;

void Consume(uint64 value)
{
    g_variantSink += value;
}

// ---- Trivial type tests ----
// Variant<int, float, double, uint64> vs std::variant<int, float, double, uint64>

using TrivialHyp = Variant<int, float, double, uint64>;
using TrivialStd = std::variant<int, float, double, uint64>;

void ProfileVariantTrivialConstruction()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        TrivialHyp v(static_cast<int>(i));
        sum += static_cast<uint64>(v.Get<int>());

        TrivialHyp v2(static_cast<float>(i) * 1.5f);
        sum += static_cast<uint64>(v2.Get<float>());
    }

    Consume(sum);
}

void ProfileStdVariantTrivialConstruction()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        TrivialStd v(static_cast<int>(i));
        sum += static_cast<uint64>(std::get<int>(v));

        TrivialStd v2(static_cast<float>(i) * 1.5f);
        sum += static_cast<uint64>(std::get<float>(v2));
    }

    Consume(sum);
}

void ProfileVariantTrivialTypeSwitch()
{
    TrivialHyp v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i & 3)
        {
        case 0: v.Set<int>(static_cast<int>(i));       sum += static_cast<uint64>(v.Get<int>());    break;
        case 1: v.Set<float>(static_cast<float>(i));   sum += static_cast<uint64>(v.Get<float>());  break;
        case 2: v.Set<double>(static_cast<double>(i)); sum += static_cast<uint64>(v.Get<double>()); break;
        case 3: v.Set<uint64>(static_cast<uint64>(i)); sum += v.Get<uint64>();                      break;
        }
    }

    Consume(sum);
}

void ProfileStdVariantTrivialTypeSwitch()
{
    TrivialStd v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i & 3)
        {
        case 0: v = static_cast<int>(i);    sum += static_cast<uint64>(std::get<int>(v));    break;
        case 1: v = static_cast<float>(i);  sum += static_cast<uint64>(std::get<float>(v));  break;
        case 2: v = static_cast<double>(i); sum += static_cast<uint64>(std::get<double>(v)); break;
        case 3: v = static_cast<uint64>(i); sum += std::get<uint64>(v);                      break;
        }
    }

    Consume(sum);
}

void ProfileVariantTrivialVisit()
{
    TrivialHyp v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i & 3)
        {
        case 0: v.Set<int>(static_cast<int>(i));       break;
        case 1: v.Set<float>(static_cast<float>(i));   break;
        case 2: v.Set<double>(static_cast<double>(i)); break;
        case 3: v.Set<uint64>(static_cast<uint64>(i)); break;
        }

        v.Visit([&sum](auto &val)
        {
            sum += static_cast<uint64>(val);
        });
    }

    Consume(sum);
}

void ProfileStdVariantTrivialVisit()
{
    TrivialStd v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i & 3)
        {
        case 0: v = static_cast<int>(i);    break;
        case 1: v = static_cast<float>(i);  break;
        case 2: v = static_cast<double>(i); break;
        case 3: v = static_cast<uint64>(i); break;
        }

        std::visit([&sum](auto &val)
        {
            sum += static_cast<uint64>(val);
        }, v);
    }

    Consume(sum);
}

// ---- Non-trivial type tests ----
// Variant<int, float, ANSIString> vs std::variant<int, float, std::string>

using NonTrivialHyp = Variant<int, float, ANSIString>;
using NonTrivialStd = std::variant<int, float, std::string>;

static constexpr const char* s_testStrings[] = {
    "hello world",
    "hyperion engine variant",
    "non-trivial type allocation overhead test",
    "string branch in variant storage"
};

void ProfileVariantNonTrivialConstruction()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialHyp v(static_cast<int>(i));
        sum += static_cast<uint64>(v.Get<int>());

        NonTrivialHyp v2(ANSIString { s_testStrings[i & 3] });
        sum += v2.Get<ANSIString>().Size();
    }

    Consume(sum);
}

void ProfileStdVariantNonTrivialConstruction()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialStd v(static_cast<int>(i));
        sum += static_cast<uint64>(std::get<int>(v));

        NonTrivialStd v2(std::string { s_testStrings[i & 3] });
        sum += std::get<std::string>(v2).size();
    }

    Consume(sum);
}

void ProfileVariantNonTrivialTypeSwitch()
{
    NonTrivialHyp v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i % 3)
        {
        case 0: v.Set<int>(static_cast<int>(i));                      sum += static_cast<uint64>(v.Get<int>());    break;
        case 1: v.Set<float>(static_cast<float>(i));                  sum += static_cast<uint64>(v.Get<float>());  break;
        case 2: v.Set<ANSIString>(ANSIString(s_testStrings[i & 3]));  sum += v.Get<ANSIString>().Size();           break;
        }
    }

    Consume(sum);
}

void ProfileStdVariantNonTrivialTypeSwitch()
{
    NonTrivialStd v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i % 3)
        {
        case 0: v = static_cast<int>(i);                    sum += static_cast<uint64>(std::get<int>(v));    break;
        case 1: v = static_cast<float>(i);                  sum += static_cast<uint64>(std::get<float>(v));  break;
        case 2: v = std::string(s_testStrings[i & 3]);       sum += std::get<std::string>(v).size();          break;
        }
    }

    Consume(sum);
}

void ProfileVariantNonTrivialVisit()
{
    NonTrivialHyp v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i % 3)
        {
        case 0: v.Set<int>(static_cast<int>(i));                      break;
        case 1: v.Set<float>(static_cast<float>(i));                  break;
        case 2: v.Set<ANSIString>(ANSIString(s_testStrings[i & 3]));  break;
        }

        v.Visit([&sum](auto &val)
        {
            using T = std::decay_t<decltype(val)>;

            if constexpr (std::is_same_v<T, ANSIString>)
            {
                sum += val.Size();
            }
            else
            {
                sum += static_cast<uint64>(val);
            }
        });
    }

    Consume(sum);
}

void ProfileStdVariantNonTrivialVisit()
{
    NonTrivialStd v(0);
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        switch (i % 3)
        {
        case 0: v = static_cast<int>(i);                   break;
        case 1: v = static_cast<float>(i);                 break;
        case 2: v = std::string(s_testStrings[i & 3]);      break;
        }

        std::visit([&sum](auto &val)
        {
            using T = std::decay_t<decltype(val)>;

            if constexpr (std::is_same_v<T, std::string>)
            {
                sum += val.size();
            }
            else
            {
                sum += static_cast<uint64>(val);
            }
        }, v);
    }

    Consume(sum);
}

// ---- Non-trivial copy/move tests ----
// Tests the overhead of copying and moving variants holding non-trivial types

void ProfileVariantNonTrivialCopy()
{
    uint64 sum = 0;

    NonTrivialHyp src(ANSIString { s_testStrings[0] });

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialHyp copy { src };
        sum += copy.Get<ANSIString>().Size();
    }

    Consume(sum);
}

void ProfileStdVariantNonTrivialCopy()
{
    uint64 sum = 0;

    NonTrivialStd src(std::string { s_testStrings[0] });

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialStd copy { src };
        sum += std::get<std::string>(copy).size();
    }

    Consume(sum);
}

void ProfileVariantNonTrivialMove()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialHyp src(ANSIString { s_testStrings[i & 3] });
        NonTrivialHyp dst(std::move(src));
        sum += dst.Get<ANSIString>().Size();
    }

    Consume(sum);
}

void ProfileStdVariantNonTrivialMove()
{
    uint64 sum = 0;

    for (size_t i = 0; i < NumVariantOps; ++i)
    {
        NonTrivialStd src(std::string { s_testStrings[i & 3] });
        NonTrivialStd dst(std::move(src));
        sum += std::get<std::string>(dst).size();
    }

    Consume(sum);
}

struct SectionEntry
{
    const char *label;
    Profile::ProfileFunction function;
};

template <size_t Count>
void RunVariantSection(const char *title, const SectionEntry (&entries)[Count], size_t runsPer, size_t numIterations, size_t runsPerIteration)
{
    Array<Profile> profiles;
    profiles.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
    {
        profiles.EmplaceBack(entries[i].function);
    }

    Array<Profile *> profilePtrs;
    profilePtrs.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
    {
        profilePtrs.PushBack(&profiles[i]);
    }

    Array<double> results = Profile::RunInterleved(std::move(profilePtrs), runsPer, numIterations, runsPerIteration);

    std::printf("%s\n", title);
    std::printf("--------\n");

    for (size_t i = 0; i < Count; ++i)
    {
        std::printf("%-36s : %.6f s\n", entries[i].label, results[i]);
    }

    std::printf("\n");
}

} // namespace

HYP_EXPORT void PrintVariantProfiling(size_t runsPer = 5, size_t numIterations = 50, size_t runsPerIteration = 10)
{
    std::printf("=== Variant: Trivial Types (int, float, double, uint64) ===\n\n");

    const SectionEntry trivialConstructionEntries[] = {
        { "Variant<int,float,double,uint64>",      &ProfileVariantTrivialConstruction },
        { "std::variant<int,float,double,uint64>", &ProfileStdVariantTrivialConstruction }
    };

    const SectionEntry trivialTypeSwitchEntries[] = {
        { "Variant<int,float,double,uint64>",      &ProfileVariantTrivialTypeSwitch },
        { "std::variant<int,float,double,uint64>", &ProfileStdVariantTrivialTypeSwitch }
    };

    const SectionEntry trivialVisitEntries[] = {
        { "Variant<int,float,double,uint64>",      &ProfileVariantTrivialVisit },
        { "std::variant<int,float,double,uint64>", &ProfileStdVariantTrivialVisit }
    };

    RunVariantSection("Construction",  trivialConstructionEntries, runsPer, numIterations, runsPerIteration);
    RunVariantSection("Type Switch",   trivialTypeSwitchEntries,   runsPer, numIterations, runsPerIteration);
    RunVariantSection("Visit",         trivialVisitEntries,        runsPer, numIterations, runsPerIteration);

    std::printf("=== Variant: Non-trivial Types (int, float, ANSIString / std::string) ===\n\n");

    const SectionEntry nonTrivialConstructionEntries[] = {
        { "Variant<int,float,ANSIString>",      &ProfileVariantNonTrivialConstruction },
        { "std::variant<int,float,std::string>", &ProfileStdVariantNonTrivialConstruction }
    };

    const SectionEntry nonTrivialTypeSwitchEntries[] = {
        { "Variant<int,float,ANSIString>",      &ProfileVariantNonTrivialTypeSwitch },
        { "std::variant<int,float,std::string>", &ProfileStdVariantNonTrivialTypeSwitch }
    };

    const SectionEntry nonTrivialVisitEntries[] = {
        { "Variant<int,float,ANSIString>",      &ProfileVariantNonTrivialVisit },
        { "std::variant<int,float,std::string>", &ProfileStdVariantNonTrivialVisit }
    };

    const SectionEntry nonTrivialCopyEntries[] = {
        { "Variant<int,float,ANSIString>",      &ProfileVariantNonTrivialCopy },
        { "std::variant<int,float,std::string>", &ProfileStdVariantNonTrivialCopy }
    };

    const SectionEntry nonTrivialMoveEntries[] = {
        { "Variant<int,float,ANSIString>",      &ProfileVariantNonTrivialMove },
        { "std::variant<int,float,std::string>", &ProfileStdVariantNonTrivialMove }
    };

    RunVariantSection("Construction",  nonTrivialConstructionEntries, runsPer, numIterations, runsPerIteration);
    RunVariantSection("Type Switch",   nonTrivialTypeSwitchEntries,   runsPer, numIterations, runsPerIteration);
    RunVariantSection("Visit",         nonTrivialVisitEntries,        runsPer, numIterations, runsPerIteration);
    RunVariantSection("Copy",          nonTrivialCopyEntries,         runsPer, numIterations, runsPerIteration);
    RunVariantSection("Move",          nonTrivialMoveEntries,         runsPer, numIterations, runsPerIteration);
}

} // namespace profiling
} // namespace tests
} // namespace Hyperion

#endif
