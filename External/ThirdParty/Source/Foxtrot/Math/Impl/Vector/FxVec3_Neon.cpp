#include <Core/FxDefines.hpp>

#ifdef FX_USE_NEON

#include <ThirdParty/Jolt/Jolt.h>
#include <ThirdParty/Jolt/Math/Real.h>
#include <arm_neon.h>

#include <Core/FxLog.hpp>
#include <Math/FxMathUtil.hpp>
#include <Math/FxNeonUtil.hpp>
#include <Math/FxVec3.hpp>
#include <Math/FxVec4.hpp>

const FxVec3f FxVec3f::sZero = FxVec3f(0.0f, 0.0f, 0.0f);
const FxVec3f FxVec3f::sOne = FxVec3f(1.0f, 1.0f, 1.0f);

const FxVec3f FxVec3f::sUp = FxVec3f(0.0f, 1.0f, 0.0f);
const FxVec3f FxVec3f::sRight = FxVec3f(1.0f, 0.0f, 0.0f);
const FxVec3f FxVec3f::sForward = FxVec3f(0.0f, 0.0f, 1.0f);


FxVec3f::FxVec3f(const JPH::Vec3& other) { FromJoltVec3(other); }
FxVec3f::FxVec3f(const FxVec4f& other) { mIntrin = other.mIntrin; }


void FxVec3f::Print() const { FxLogInfo("Vec3f {{ X={:.6f}, Y={:.6f}, Z={:.6f} }}", X, Y, Z); }


FxVec3f FxVec3f::CrossSlow(const FxVec3f& other) const
{
    const float32 ax = mData[0];
    const float32 bx = other.mData[0];

    const float32 ay = mData[1];
    const float32 by = other.mData[1];

    const float32 az = mData[2];
    const float32 bz = other.mData[2];

    return FxVec3f(ay * bz - by * az, az * bx - bz * ax, ax * by - bx * ay);
}

FxVec3f FxVec3f::Cross(const FxVec3f& other) const
{
    const float32x4_t a = mIntrin;
    const float32x4_t b = other.mIntrin;

    float32x4_t a_yzxw = FxNeon::Permute4<FxShuffle_AY, FxShuffle_AZ, FxShuffle_AX, FxShuffle_AW>(a);
    float32x4_t b_yzxw = FxNeon::Permute4<FxShuffle_AY, FxShuffle_AZ, FxShuffle_AX, FxShuffle_AW>(b);

    const float32x4_t result_yzxw = vsubq_f32(vmulq_f32(a, b_yzxw), vmulq_f32(a_yzxw, b));

    return FxVec3f(FxNeon::Permute4<FxShuffle_AY, FxShuffle_AZ, FxShuffle_AX, FxShuffle_AW>(result_yzxw));
}

void FxVec3f::ToJoltVec3(JPH::RVec3& jolt_vec) const { jolt_vec.mValue = mIntrin; }
void FxVec3f::FromJoltVec3(const JPH::RVec3& jolt_vec) { mIntrin = jolt_vec.mValue; }

bool FxVec3f::IsCloseTo(const JPH::Vec3& other, const float32 tolerance) const
{
    return IsCloseTo(other.mValue, tolerance);
}


bool FxVec3f::operator==(const JPH::Vec3& other) const { return (*this) == FxVec3f(other.mValue); }

#endif
