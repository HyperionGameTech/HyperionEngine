#include <Core/FxDefines.hpp>

#ifdef FX_NO_SIMD

#include <ThirdParty/Jolt/Jolt.h>
#include <ThirdParty/Jolt/Math/Real.h>

#include <Math/FxVec3.hpp>

const FxVec3f FxVec3f::sZero = FxVec3f(0.0f, 0.0f, 0.0f);
const FxVec3f FxVec3f::sOne = FxVec3f(1.0f, 1.0f, 1.0f);

const FxVec3f FxVec3f::sUp = FxVec3f(0.0f, 1.0f, 0.0f);
const FxVec3f FxVec3f::sRight = FxVec3f(1.0f, 0.0f, 0.0f);
const FxVec3f FxVec3f::sForward = FxVec3f(0.0f, 0.0f, 1.0f);

FxVec3f::FxVec3f(const JPH::Vec3& other) { FromJoltVec3(other); }


void FxVec3f::Print() const { FxLogInfo("Vec3f {{ X={:.6f}, Y={:.6f}, Z={:.6f} }}", X, Y, Z); }

float32 FxVec3f::Length() const
{
    const float32 len2 = (X * X) + (Y * Y) + (Z * Z);
    return sqrtf(len2);
}

FxVec3f FxVec3f::Normalize() const
{
    // Calculate length
    const float32 len = Length();

    return FxVec3f(X / len, Y / len, Z / len);
}

FxVec3f FxVec3f::CrossSlow(const FxVec3f& other) const { return Cross(other); }

FxVec3f FxVec3f::Cross(const FxVec3f& other) const
{
    const float32 ax = X;
    const float32 bx = other.X;

    const float32 ay = Y;
    const float32 by = other.Y;

    const float32 az = Z;
    const float32 bz = other.Z;

    return FxVec3f(ay * bz - by * az, az * bx - bz * ax, ax * by - bx * ay);
}

float32 FxVec3f::Dot(const FxVec3f& other) const
{
    const float32 result = (X * other.X) + (Y * other.Y) + (Z * other.Z);
    return result;
}

void FxVec3f::ToJoltVec3(JPH::RVec3& jolt_vec) const
{
    jolt_vec.SetX(X);
    jolt_vec.SetY(Y);
    jolt_vec.SetZ(Z);
}

void FxVec3f::FromJoltVec3(const JPH::RVec3& jolt_vec)
{
    X = jolt_vec.GetX();
    Y = jolt_vec.GetY();
    Z = jolt_vec.GetZ();
}


bool FxVec3f::IsCloseTo(const JPH::Vec3& other, const float32 threshold) const
{
    return IsCloseTo(FxVec3f(other), threshold);
}


//////////////////////////////
// Operator Overloads
//////////////////////////////


bool FxVec3f::operator==(const JPH::Vec3& other) const { return (*this) == FxVec3f(other); }

#endif
