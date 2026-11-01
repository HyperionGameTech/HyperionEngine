/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Utilities/Float16.hpp>
#include <Core/Utilities/Format.hpp>

#include <bit>
#include <cmath>

namespace Hyperion {
namespace tests {
namespace core {

static_assert(Float16(1.0f).Raw() == 0x3C00);
static_assert(Float16(-2.0f).Raw() == 0xC000);
static_assert(Float16(65504.0f).Raw() == 0x7BFF);
static_assert(Float16(65519.0f).Raw() == 0x7BFF);
static_assert(Float16(65520.0f).Raw() == 0x7C00);
static_assert(Float16(5.9604645e-8f).Raw() == 0x0001);
static_assert(Float16(2.9802322e-8f).Raw() == 0x0000);
static_assert(float(Float16::FromRaw(0x0001)) == 5.9604645e-8f);
static_assert(float(Float16::FromRaw(0x3555)) == 0.33325195f);
static_assert((-Float16(0.0f)).Raw() == 0x8000);

static_assert(sizeof(Vec4h) == 8 && alignof(Vec4h) == 8);
static_assert(Vec4h(Vec4f(1.0f, 2.0f, 3.0f, 4.0f)).z.Raw() == 0x4200);
static_assert(Vec4f(Vec4h(Float16(0.5f))).w == 0.5f);
static_assert(Vec4h::One().x.Raw() == 0x3C00 && Vec4h::Zero().IsZero());

namespace {

constexpr uint32 FloatSignMask = 0x80000000u;

bool IsHalfNan(uint16 half)
{
    return (half & 0x7FFFu) > 0x7C00u;
}

bool IsFloatNan(float value)
{
    return (std::bit_cast<uint32>(value) & ~FloatSignMask) > 0x7F800000u;
}

bool SameHalf(uint16 expected, uint16 actual)
{
    if (IsHalfNan(expected))
    {
        return IsHalfNan(actual) && (expected & 0x8000u) == (actual & 0x8000u);
    }

    return expected == actual;
}

bool SameFloat(float expected, float actual)
{
    if (IsFloatNan(expected))
    {
        return IsFloatNan(actual) && (std::bit_cast<uint32>(expected) & FloatSignMask) == (std::bit_cast<uint32>(actual) & FloatSignMask);
    }

    return std::bit_cast<uint32>(expected) == std::bit_cast<uint32>(actual);
}

// Exact conversions in double precision, slow but plainly correct
uint16 ReferenceToHalf(float value)
{
    const uint32 bits = std::bit_cast<uint32>(value);
    const uint16 sign = uint16((bits & FloatSignMask) >> 16);

    if (IsFloatNan(value))
    {
        return uint16(sign | 0x7E00u);
    }

    const double magnitude = std::fabs(double(value));

    // halfway between 65504 and 65536 ties to the even 65536, which is infinity
    if (magnitude >= 65520.0)
    {
        return uint16(sign | 0x7C00u);
    }

    if (magnitude == 0.0)
    {
        return sign;
    }

    int exponent = 0;
    std::frexp(magnitude, &exponent);

    const int quantumExponent = (exponent - 1 < -14 ? -14 : exponent - 1) - 10;
    const int steps = int(std::nearbyint(std::ldexp(magnitude, -quantumExponent)));

    if (exponent - 1 < -14)
    {
        return uint16(sign | steps);
    }

    return uint16(sign | (((exponent - 1 + 15) << 10) + steps - 1024));
}

float ReferenceToFloat(uint16 half)
{
    const uint32 sign = uint32(half & 0x8000u) << 16;
    const uint32 exponent = (half >> 10) & 0x1Fu;
    const uint32 mantissa = half & 0x3FFu;

    if (exponent == 31)
    {
        return std::bit_cast<float>(sign | 0x7F800000u | (mantissa << 13));
    }

    double magnitude = 0.0;

    if (exponent == 0)
    {
        magnitude = std::ldexp(double(mantissa), -24);
    }
    else
    {
        magnitude = std::ldexp(double(1024 + mantissa), int(exponent) - 25);
    }

    return std::bit_cast<float>(sign | std::bit_cast<uint32>(float(magnitude)));
}

struct Mismatches
{
    size_t count = 0;
    uint32 input = 0;
    uint32 expected = 0;
    uint32 actual = 0;

    void Add(uint32 inputBits, uint32 expectedBits, uint32 actualBits)
    {
        if (count++ == 0)
        {
            input = inputBits;
            expected = expectedBits;
            actual = actualBits;
        }
    }
};

void Report(EngineTestContext& context, const char* path, const Mismatches& mismatches)
{
    context.Check(path, mismatches.count == 0,
        HYP_FORMAT("{} mismatches, first: input bits {} expected bits {} got bits {}", mismatches.count, mismatches.input, mismatches.expected, mismatches.actual));
}

void CheckFloatsToHalves(EngineTestContext& context, Span<const float> samples)
{
    Array<Float16> converted;
    converted.Resize(samples.Size());

    Float16::FromFloatArray(samples, Span<Float16>(converted.Data(), converted.Size()));

    Mismatches viaConstructor;
    Mismatches viaSoftware;
    Mismatches viaFloat4;
    Mismatches viaArray;

    for (size_t index = 0; index < samples.Size(); index++)
    {
        const float sample = samples.Data()[index];
        const uint32 bits = std::bit_cast<uint32>(sample);
        const uint16 expected = ReferenceToHalf(sample);
        const uint16 expectedNegated = ReferenceToHalf(std::bit_cast<float>(bits ^ FloatSignMask));

        const uint16 fromConstructor = Float16(sample).Raw();
        if (!SameHalf(expected, fromConstructor))
        {
            viaConstructor.Add(bits, expected, fromConstructor);
        }

        const uint16 fromSoftware = Float16::FloatBitsToHalf(bits);
        if (!SameHalf(expected, fromSoftware))
        {
            viaSoftware.Add(bits, expected, fromSoftware);
        }

        const float lanesIn[4] = { sample, std::bit_cast<float>(bits ^ FloatSignMask), 1.0f, sample };
        Float16 lanesOut[4];
        Float16::FromFloat4(lanesIn, lanesOut);

        if (!SameHalf(expected, lanesOut[0].Raw()) || !SameHalf(expectedNegated, lanesOut[1].Raw()) || lanesOut[2].Raw() != 0x3C00u || !SameHalf(expected, lanesOut[3].Raw()))
        {
            viaFloat4.Add(bits, expected, lanesOut[0].Raw());
        }

        if (!SameHalf(expected, converted[index].Raw()))
        {
            viaArray.Add(bits, expected, converted[index].Raw());
        }
    }

    Report(context, "Float16(float)", viaConstructor);
    Report(context, "Float16::FloatBitsToHalf", viaSoftware);
    Report(context, "Float16::FromFloat4", viaFloat4);
    Report(context, "Float16::FromFloatArray", viaArray);
}

uint32 NextRandom(uint32& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

} // namespace

HYP_ENGINE_TEST(Float16, EveryHalfToFloat)
{
    Array<Float16> halves;
    halves.Resize(65536);

    for (uint32 raw = 0; raw < 65536; raw++)
    {
        halves[raw] = Float16::FromRaw(uint16(raw));
    }

    Array<float> converted;
    converted.Resize(65536);

    Float16::ToFloatArray(Span<const Float16>(halves.Data(), halves.Size()), Span<float>(converted.Data(), converted.Size()));

    Mismatches viaOperator;
    Mismatches viaSoftware;
    Mismatches viaFloat4;
    Mismatches viaArray;

    for (uint32 raw = 0; raw < 65536; raw++)
    {
        const float expected = ReferenceToFloat(uint16(raw));
        const uint32 expectedBits = std::bit_cast<uint32>(expected);

        const float fromOperator = float(halves[raw]);
        if (!SameFloat(expected, fromOperator))
        {
            viaOperator.Add(raw, expectedBits, std::bit_cast<uint32>(fromOperator));
        }

        const float fromSoftware = Float16::HalfToFloat(uint16(raw));
        if (!SameFloat(expected, fromSoftware))
        {
            viaSoftware.Add(raw, expectedBits, std::bit_cast<uint32>(fromSoftware));
        }

        const Float16 lanesIn[4] = { halves[raw], Float16::FromRaw(uint16(raw ^ 0x8000u)), Float16::FromRaw(0x3C00), halves[raw] };
        float lanesOut[4];
        Float16::ToFloat4(lanesIn, lanesOut);

        if (!SameFloat(expected, lanesOut[0]) || !SameFloat(ReferenceToFloat(uint16(raw ^ 0x8000u)), lanesOut[1]) || lanesOut[2] != 1.0f || !SameFloat(expected, lanesOut[3]))
        {
            viaFloat4.Add(raw, expectedBits, std::bit_cast<uint32>(lanesOut[0]));
        }

        if (!SameFloat(expected, converted[raw]))
        {
            viaArray.Add(raw, expectedBits, std::bit_cast<uint32>(converted[raw]));
        }
    }

    Report(context, "Float16 -> float", viaOperator);
    Report(context, "Float16::HalfToFloat", viaSoftware);
    Report(context, "Float16::ToFloat4", viaFloat4);
    Report(context, "Float16::ToFloatArray", viaArray);
}

HYP_ENGINE_TEST(Float16, RoundsToNearestEvenAtEveryBoundary)
{
    Array<float> samples;
    samples.Reserve(0x7C00 * 8);

    for (uint32 raw = 0; raw < 0x7C00; raw++)
    {
        const double lower = double(ReferenceToFloat(uint16(raw)));
        const double upper = raw == 0x7BFF ? 65536.0 : double(ReferenceToFloat(uint16(raw + 1)));
        const float midpoint = float((lower + upper) * 0.5);

        const float candidates[4] = {
            float(lower),
            midpoint,
            std::nextafter(midpoint, 0.0f),
            std::nextafter(midpoint, INFINITY)
        };

        for (float candidate : candidates)
        {
            samples.PushBack(candidate);
            samples.PushBack(std::bit_cast<float>(std::bit_cast<uint32>(candidate) ^ FloatSignMask));
        }
    }

    CheckFloatsToHalves(context, Span<const float>(samples.Data(), samples.Size()));
}

HYP_ENGINE_TEST(Float16, RandomFloatsToHalf)
{
    Array<float> samples;
    samples.Reserve(1 << 21);

    uint32 state = 0x2545F491u;

    for (uint32 index = 0; index < (1u << 20); index++)
    {
        samples.PushBack(std::bit_cast<float>(NextRandom(state)));
    }

    // exponents from below the smallest half subnormal up past the largest half
    for (uint32 index = 0; index < (1u << 20); index++)
    {
        const uint32 bits = NextRandom(state);
        const uint32 exponent = 96 + (bits >> 8) % 56;

        samples.PushBack(std::bit_cast<float>((bits & FloatSignMask) | (exponent << 23) | (NextRandom(state) & 0x7FFFFFu)));
    }

    CheckFloatsToHalves(context, Span<const float>(samples.Data(), samples.Size()));
}

HYP_ENGINE_TEST(Float16, SpecialValues)
{
    const float samples[] = {
        0.0f,
        -0.0f,
        INFINITY,
        -INFINITY,
        std::bit_cast<float>(0x7FC00000u),
        std::bit_cast<float>(0xFF800001u),
        std::bit_cast<float>(0x7F7FFFFFu),
        std::bit_cast<float>(0x00000001u),
        std::bit_cast<float>(0x807FFFFFu),
        65504.0f,
        65519.996f,
        65520.0f,
        -65520.0f,
        6.1035156e-5f,
        6.0975552e-5f,
        2.9802322e-8f,
        2.9802326e-8f
    };

    CheckFloatsToHalves(context, Span<const float>(samples, sizeof(samples) / sizeof(samples[0])));

    HYP_TEST_CHECK(Float16(65520.0f).Raw() == 0x7C00);
    HYP_TEST_CHECK(Float16(-INFINITY).Raw() == 0xFC00);
    HYP_TEST_CHECK(Float16(2.9802322e-8f).Raw() == 0x0000);
    HYP_TEST_CHECK(Float16(2.9802326e-8f).Raw() == 0x0001);
    HYP_TEST_CHECK(IsHalfNan(Float16(std::bit_cast<float>(0x7FC00000u)).Raw()));
    HYP_TEST_CHECK(IsFloatNan(float(Float16::FromRaw(0x7E00))));
    HYP_TEST_CHECK(float(Float16::FromRaw(0x7C00)) == INFINITY);
}

HYP_ENGINE_TEST(Float16, ArithmeticAndComparison)
{
    HYP_TEST_CHECK(float(Float16(1.5f) + Float16(2.25f)) == 3.75f);
    HYP_TEST_CHECK(float(Float16(1.5f) - Float16(2.25f)) == -0.75f);
    HYP_TEST_CHECK(float(Float16(1.5f) * Float16(-2.0f)) == -3.0f);
    HYP_TEST_CHECK(float(Float16(3.0f) / Float16(4.0f)) == 0.75f);

    HYP_TEST_CHECK((-Float16(1.0f)).Raw() == 0xBC00);
    HYP_TEST_CHECK((-Float16(-0.0f)).Raw() == 0x0000);

    HYP_TEST_CHECK(Float16(0.0f) == Float16(-0.0f));
    HYP_TEST_CHECK(Float16(-1.0f) < Float16(0.5f));
    HYP_TEST_CHECK(Float16(2.0f) >= Float16(2.0f));

    Float16 counter = 1.0f;
    counter++;
    ++counter;
    counter *= 2.0f;
    HYP_TEST_CHECK(float(counter) == 6.0f);
}

HYP_ENGINE_TEST(Float16, ArrayConversionsStopAtTheShorterSpan)
{
    float floats[19];
    for (uint32 index = 0; index < 19; index++)
    {
        floats[index] = float(index) * 0.5f - 3.0f;
    }

    for (uint32 count = 0; count <= 19; count++)
    {
        Float16 halves[19];
        for (Float16& half : halves)
        {
            half = Float16::FromRaw(0x1234);
        }

        Float16::FromFloatArray(Span<const float>(floats, count), Span<Float16>(halves, 19));

        float roundTrip[19];
        for (float& value : roundTrip)
        {
            value = 123.0f;
        }

        Float16::ToFloatArray(Span<const Float16>(halves, 19), Span<float>(roundTrip, count));

        bool ok = true;

        for (uint32 index = 0; index < 19; index++)
        {
            if (index < count)
            {
                ok = ok && halves[index].Raw() == Float16(floats[index]).Raw() && roundTrip[index] == floats[index];
            }
            else
            {
                ok = ok && halves[index].Raw() == 0x1234 && roundTrip[index] == 123.0f;
            }
        }

        HYP_TEST_CHECK_MSG(ok, "{} values: wrote outside the shorter span or converted wrong", count);
    }
}

HYP_ENGINE_TEST(Vec4h, ConvertsToAndFromVec4f)
{
    const Vec4f source(1.0f, -2.5f, 0.33325195f, 65504.0f);
    const Vec4h half(source);

    HYP_TEST_CHECK(half.x.Raw() == Float16(source.x).Raw());
    HYP_TEST_CHECK(half.y.Raw() == Float16(source.y).Raw());
    HYP_TEST_CHECK(half.z.Raw() == Float16(source.z).Raw());
    HYP_TEST_CHECK(half.w.Raw() == Float16(source.w).Raw());

    const Vec4f back(half);
    HYP_TEST_CHECK(back.x == source.x && back.y == source.y && back.z == source.z && back.w == source.w);

    const Vec4h fromXyz(Vec3f(1.0f, 2.0f, 3.0f), 4.0f);
    const Vec3f xyz = fromXyz.GetXYZ();
    HYP_TEST_CHECK(xyz.x == 1.0f && xyz.y == 2.0f && xyz.z == 3.0f && float(fromXyz.w) == 4.0f);

    const Vec4h rounded(Vec4f(1.0f / 3.0f, 70000.0f, -1e-9f, 2049.0f));
    HYP_TEST_CHECK(rounded.x.Raw() == 0x3555);
    HYP_TEST_CHECK(rounded.y.Raw() == 0x7C00);
    HYP_TEST_CHECK(rounded.z.Raw() == 0x8000);
    HYP_TEST_CHECK(rounded.w.Raw() == 0x6800);
}

HYP_ENGINE_TEST(Vec4h, Arithmetic)
{
    const Vec4h a(Vec4f(1.0f, -2.0f, 0.25f, 100.0f));
    const Vec4h b(Vec4f(0.5f, 4.0f, 0.75f, -50.0f));

    const auto matches = [](const Vec4h& value, float x, float y, float z, float w)
    {
        const Vec4f asFloat(value);
        return asFloat.x == x && asFloat.y == y && asFloat.z == z && asFloat.w == w;
    };

    HYP_TEST_CHECK(matches(a + b, 1.5f, 2.0f, 1.0f, 50.0f));
    HYP_TEST_CHECK(matches(a - b, 0.5f, -6.0f, -0.5f, 150.0f));
    HYP_TEST_CHECK(matches(a * b, 0.5f, -8.0f, 0.1875f, -5000.0f));
    HYP_TEST_CHECK(matches(a / b, 2.0f, -0.5f, 0.33325195f, -2.0f));
    HYP_TEST_CHECK(matches(-a, -1.0f, 2.0f, -0.25f, -100.0f));

    Vec4h accumulated = a;
    accumulated += b;
    accumulated *= Vec4h(Float16(2.0f));
    accumulated -= Vec4h::One();
    accumulated /= Vec4h(Float16(0.5f));
    HYP_TEST_CHECK(matches(accumulated, 4.0f, 6.0f, 2.0f, 198.0f));
}

HYP_ENGINE_TEST(Vec4h, EqualityZeroAndHash)
{
    const Vec4h a(Vec4f(1.0f, 2.0f, 3.0f, 4.0f));
    const Vec4h same(Float16(1.0f), Float16(2.0f), Float16(3.0f), Float16(4.0f));
    const Vec4h other(Vec4f(1.0f, 2.0f, 3.0f, 5.0f));

    HYP_TEST_CHECK(a == same);
    HYP_TEST_CHECK(a != other);
    HYP_TEST_CHECK(a.GetHashCode() == same.GetHashCode());
    HYP_TEST_CHECK(a.GetHashCode() != other.GetHashCode());

    HYP_TEST_CHECK(Vec4h().IsZero());
    HYP_TEST_CHECK(Vec4h(Float16(-0.0f)).IsZero());
    HYP_TEST_CHECK(!Vec4h::One().IsZero());
    HYP_TEST_CHECK(!Vec4h(Float16(0.0f), Float16(0.0f), Float16(0.0f), Float16::FromRaw(0x0001)).IsZero());

    Vec4h indexed;
    indexed[2] = Float16(7.0f);
    HYP_TEST_CHECK(float(indexed.z) == 7.0f && float(indexed[2]) == 7.0f);
}

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
