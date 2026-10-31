/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/Ragdoll.hpp>
#include <ActiveRagdoll/RagdollBalance.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Core/Math/Color.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float KinematicDim = 0.45f;
static constexpr float MinVelocityDraw = 0.5f;
static constexpr float VelocityScale = 0.15f;
static constexpr float ComCrossSize = 0.25f;
static constexpr float ComGroundCrossSize = 0.12f;
static constexpr float MarkerSize = 0.06f;

static constexpr Color PartColors[] = {
    Color(0.92f, 0.92f, 0.92f),
    Color(1.0f, 0.24f, 0.24f),
    Color(0.27f, 0.67f, 1.0f),
    Color(0.31f, 0.9f, 0.35f)
};

static constexpr Color LinkColor = Color(1.0f, 0.86f, 0.24f);
static constexpr Color VelocityColor = Color(1.0f, 0.27f, 0.86f);
static constexpr Color ComColor = Color(1.0f, 0.55f, 0.0f);
static constexpr Color SupportColor = Color(0.35f, 0.86f, 1.0f);
static constexpr Color CaptureColor = Color(1.0f, 0.9f, 0.16f);
static constexpr Color CaptureOutsideColor = Color(1.0f, 0.24f, 0.16f);
static constexpr Color SwingColor = Color(1.0f, 0.27f, 0.86f);

Color GetTorqueColor(float fraction)
{
    const float t = MathUtil::Clamp(fraction, 0.0f, 1.0f);

    return Color(0.27f + 0.73f * t, 0.9f - 0.67f * t, 0.35f);
}

Color Dim(const Color& color, float factor)
{
    return Color(color.GetRed() * factor, color.GetGreen() * factor, color.GetBlue() * factor);
}

void DrawLine(
    DebugDrawCommandList& list,
    JPH::Vec3Arg from,
    JPH::Vec3Arg to,
    const Color& color)
{
    list.line(FromJolt(from), FromJolt(to), color);
}

void DrawGroundCross(
    DebugDrawCommandList& list,
    JPH::Vec3Arg point,
    JPH::Vec3Arg axisA,
    JPH::Vec3Arg axisB,
    float size,
    const Color& color)
{
    DrawLine(list, point - axisA * size, point + axisA * size, color);
    DrawLine(list, point - axisB * size, point + axisB * size, color);
}

void DrawWireBox(
    DebugDrawCommandList& list,
    JPH::Vec3Arg center,
    JPH::Vec3Arg x,
    JPH::Vec3Arg y,
    JPH::Vec3Arg z,
    const Color& color)
{
    JPH::Vec3 corners[8];

    for (uint32 i = 0; i < 8; i++)
    {
        const float sx = (i & 1) ? 1.0f : -1.0f;
        const float sy = (i & 2) ? 1.0f : -1.0f;
        const float sz = (i & 4) ? 1.0f : -1.0f;

        corners[i] = center + x * sx + y * sy + z * sz;
    }

    for (uint32 i = 0; i < 8; i++)
    {
        for (uint32 bit = 1; bit < 8; bit <<= 1)
        {
            if (!(i & bit))
            {
                DrawLine(list, corners[i], corners[i | bit], color);
            }
        }
    }
}

} // namespace

void Ragdoll::DebugDraw(DebugDrawCommandList& list) const
{
    if (m_ragdoll == nullptr)
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = GetBodyInterface();

    Array<BodyState, DebugDrawAllocator> samples;
    samples.Resize(m_bodies.Size());

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        SampleBody(bodyInterface, i, samples[i]);
    }

    Array<JPH::Vec3, DebugDrawAllocator> centers;
    centers.Resize(m_bodies.Size());

    DrawDebugBodies(list, samples.ToSpan(), centers.ToSpan());
    DrawDebugLinks(list, centers.ToSpan());
    DrawDebugSupport(list, samples.ToSpan());

    if (m_isBalancing)
    {
        m_balance.DebugDraw(list);
    }
}

void Ragdoll::DrawDebugBodies(DebugDrawCommandList& list, Span<const BodyState> samples, Span<JPH::Vec3> outCenters) const
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        const BodyInfo& info = m_bodies[i];
        const BodyState& sample = samples[i];

        const JPH::Mat44 rotation = JPH::Mat44::sRotation(sample.orientation);

        const JPH::Vec3 axis = rotation.Multiply3x3(info.localAxis);
        const JPH::Vec3 center = sample.origin + rotation.Multiply3x3(info.localCenter);
        const JPH::Vec3 side = axis.GetNormalizedPerpendicular();
        const JPH::Vec3 front = side.Cross(axis);

        JPH::Vec3 x = side * info.radius;
        JPH::Vec3 y = axis * (info.length * 0.5f);
        JPH::Vec3 z = front * info.radius;

        if (info.isBox)
        {
            const JPH::Mat44 boxRotation = JPH::Mat44::sRotation(sample.orientation * info.boxRotation);

            x = boxRotation.GetAxisX() * info.boxHalfExtent.GetX();
            y = boxRotation.GetAxisY() * info.boxHalfExtent.GetY();
            z = boxRotation.GetAxisZ() * info.boxHalfExtent.GetZ();
        }

        outCenters[i] = center;

        Color color = PartColors[uint32(info.part)];

        if (info.mode == RagdollBodyMode::Kinematic)
        {
            color = Dim(color, KinematicDim);
        }

        DrawWireBox(list, center, x, y, z, color);

        if (sample.velocity.Length() > MinVelocityDraw)
        {
            DrawLine(list, center, center + sample.velocity * VelocityScale, VelocityColor);
        }
    }
}

void Ragdoll::DrawDebugLinks(DebugDrawCommandList& list, Span<const JPH::Vec3> centers) const
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        const int32 parent = m_bodies[i].parentBody;

        if (parent < 0)
        {
            continue;
        }

        Color linkColor = LinkColor;

        if (m_bodies[i].mode == RagdollBodyMode::Powered)
        {
            linkColor = GetTorqueColor(GetMotorTorqueFraction(i));
        }

        DrawLine(list, centers[i], centers[parent], linkColor);
    }
}

void Ragdoll::DrawDebugSupport(DebugDrawCommandList& list, Span<const BodyState> samples) const
{
    const GravityFrame frame = GetGravityFrame();
    const JPH::Vec3 up = frame.up;

    JPH::Vec3 com;
    JPH::Vec3 velocity;

    if (!GetMassAverage(samples, com, velocity))
    {
        return;
    }

    DrawLine(list, com - frame.axisU * ComCrossSize, com + frame.axisU * ComCrossSize, ComColor);
    DrawLine(list, com - up * ComCrossSize, com + up * ComCrossSize, ComColor);
    DrawLine(list, com - frame.axisV * ComCrossSize, com + frame.axisV * ComCrossSize, ComColor);

    Array<PlanePoint, DebugDrawAllocator> supportPoints;
    float supportHeightSum = 0.0f;

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        RagdollGroundContact contact;

        if (!GetBodyGroundContact(i, contact) || !IsSupportNormal(contact, up, SupportNormalMinUp))
        {
            continue;
        }

        supportPoints.PushBack(frame.ToPlane(contact.point));
        supportHeightSum += contact.point.Dot(up);
    }

    if (supportPoints.Empty())
    {
        return;
    }

    const float supportHeight = supportHeightSum / float(supportPoints.Size());
    const JPH::Vec3 comOnSupport = com - up * (com.Dot(up) - supportHeight);

    DrawLine(list, com, comOnSupport, ComColor);
    DrawGroundCross(list, comOnSupport, frame.axisU, frame.axisV, ComGroundCrossSize, ComColor);

    Array<PlanePoint, DebugDrawAllocator> hull;
    hull.Resize(supportPoints.Size() + 1);

    const uint32 hullCount = BuildHull(supportPoints.Data(), uint32(supportPoints.Size()), hull.Data());

    if (hullCount < 2)
    {
        return;
    }

    for (uint32 i = 0; i < hullCount; i++)
    {
        DrawLine(list, frame.FromPlane(hull[i], supportHeight), frame.FromPlane(hull[(i + 1) % hullCount], supportHeight), SupportColor);
    }
}

void RagdollBalance::DebugDraw(DebugDrawCommandList& list) const
{
    if (!m_isValid || m_status.state == RagdollBalanceState::Off)
    {
        return;
    }

    const JPH::Vec3 side = m_frame.up.Cross(m_forward);

    Color captureColor = CaptureColor;

    if (m_captureOutside > 0.0f)
    {
        captureColor = CaptureOutsideColor;
    }

    DrawGroundCross(list, m_capturePoint, m_forward, side, MarkerSize, captureColor);
    DrawGroundCross(list, m_supportCenter, m_forward, side, MarkerSize, SupportColor);
    DrawLine(list, ToGround(m_centerOfMass), m_capturePoint, captureColor);

    if (m_status.state == RagdollBalanceState::Stepping)
    {
        DrawGroundCross(list, m_swing.landing, m_forward, side, MarkerSize, SwingColor);
        DrawLine(list, m_legs[m_swing.leg].anklePosition, m_swing.ankleTarget, SwingColor);
    }
}

} // namespace Hyperion
