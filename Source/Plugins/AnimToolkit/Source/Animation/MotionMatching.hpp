/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Animation/AnimationGraph.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
class Skeleton;

HYP_STRUCT()
struct MotionDatabaseClip
{
    HYP_STRUCT_BODY(MotionDatabaseClip);

    HYP_FIELD(Property = "Clip", Serialize = true, Editor = true)
    Handle<Animation> clip;

    HYP_FIELD(Property = "Velocity", Serialize = true, Editor = true)
    Vec2f velocity;

    HYP_FIELD(Property = "Loop", Serialize = true, Editor = true)
    bool loop = true;
};

HYP_STRUCT()
struct MotionFeatureWeights
{
    HYP_STRUCT_BODY(MotionFeatureWeights);

    HYP_FIELD(Property = "FootPosition", Serialize = true, Editor = true)
    float footPosition = 0.75f;

    HYP_FIELD(Property = "FootVelocity", Serialize = true, Editor = true)
    float footVelocity = 0.5f;

    HYP_FIELD(Property = "HipVelocity", Serialize = true, Editor = true)
    float hipVelocity = 0.5f;

    HYP_FIELD(Property = "TrajectoryPosition", Serialize = true, Editor = true)
    float trajectoryPosition = 3.0f;

    HYP_FIELD(Property = "TrajectoryFacing", Serialize = true, Editor = true)
    float trajectoryFacing = 1.5f;
};

HYP_STRUCT()
struct MotionDatabaseFrame
{
    HYP_STRUCT_BODY(MotionDatabaseFrame);

    HYP_FIELD(Property = "Clip", Serialize = true)
    uint32 clipIndex = 0;

    HYP_FIELD(Property = "Time", Serialize = true)
    float time = 0.0f;
};

enum class MotionFeature : uint32
{
    LeftFootPosition = 0,
    RightFootPosition = 3,
    LeftFootVelocity = 6,
    RightFootVelocity = 9,
    HipVelocity = 12,
    TrajectoryPosition = 15,
    TrajectoryFacing = 21,
    Count = 27
};

HYP_CLASS(AssetBucket = "Animations")
class ANIMATION_API MotionDatabase final : public AssetObject
{
    HYP_OBJECT_BODY(MotionDatabase);

public:
    static constexpr uint32 NumTrajectoryPoints = 3;
    static constexpr uint32 NumFeatures = uint32(MotionFeature::Count);

    MotionDatabase();
    explicit MotionDatabase(Name name);

    ~MotionDatabase() override;

    HYP_METHOD(Property = "Skeleton")
    const Handle<Skeleton>& GetSkeleton() const
    {
        return m_skeleton;
    }

    HYP_METHOD(Property = "Skeleton")
    void SetSkeleton(const Handle<Skeleton>& skeleton)
    {
        m_skeleton = skeleton;
        ClearBuild();
    }

    HYP_METHOD(Property = "Clips", NoScriptBindings)
    const Array<MotionDatabaseClip>& GetClips() const
    {
        return m_clips;
    }

    HYP_METHOD(Property = "Clips", NoScriptBindings)
    void SetClips(const Array<MotionDatabaseClip>& clips)
    {
        m_clips = clips;
        ClearBuild();
    }

    HYP_METHOD(Property = "Weights", NoScriptBindings)
    const MotionFeatureWeights& GetWeights() const
    {
        return m_weights;
    }

    HYP_METHOD(Property = "Weights", NoScriptBindings)
    void SetWeights(const MotionFeatureWeights& weights)
    {
        m_weights = weights;
        UpdateScales();
    }

    HYP_METHOD(Property = "TrajectoryHorizon")
    float GetTrajectoryHorizon() const
    {
        return m_trajectoryHorizon;
    }

    HYP_METHOD(Property = "TrajectoryHorizon")
    void SetTrajectoryHorizon(float trajectoryHorizon)
    {
        m_trajectoryHorizon = trajectoryHorizon;
        ClearBuild();
    }

    HYP_METHOD(Property = "SampleRate")
    float GetSampleRate() const
    {
        return m_sampleRate;
    }

    HYP_METHOD(Property = "SampleRate")
    void SetSampleRate(float sampleRate)
    {
        m_sampleRate = sampleRate;
        ClearBuild();
    }

    float GetTrajectoryTime(uint32 point) const
    {
        return m_trajectoryHorizon * float(point + 1) / float(NumTrajectoryPoints);
    }

    HYP_METHOD(EditorAction = "Build Database")
    bool Build();

    HYP_METHOD()
    bool IsBuilt() const
    {
        return !m_frames.Empty() && m_features.Size() == m_frames.Size() * NumFeatures;
    }

    HYP_METHOD()
    uint32 NumFrames() const
    {
        return uint32(m_frames.Size());
    }

    const MotionDatabaseFrame& GetFrame(uint32 frame) const
    {
        return m_frames[frame];
    }

    Span<const float> GetFrameFeatures(uint32 frame) const
    {
        return Span<const float>(m_features.Data() + size_t(frame) * NumFeatures, NumFeatures);
    }

    // what a feature is multiplied by before comparing: its group's weight over the group's spread
    float GetFeatureScale(uint32 feature) const
    {
        return feature < m_scales.Size() ? m_scales[feature] : 0.0f;
    }

    // the frame of a clip nearest to a time, or ~0u when the clip has none
    uint32 FindFrame(uint32 clipIndex, float time) const;

    // the frame whose features are closest to query; with includePose false only the trajectory counts
    uint32 Search(Span<const float> query, bool includePose) const;

    // where a character moving at velocity will be at time, easing toward desiredVelocity at a rate of acceleration (1/s)
    static Vec2f PredictPosition(const Vec2f& velocity, const Vec2f& desiredVelocity, float acceleration, float time);

private:
    void OnLoaded() override;

    void UpdateScales();
    void ClearBuild();

    HYP_FIELD(Property = "Skeleton", Serialize)
    Handle<Skeleton> m_skeleton;

    HYP_FIELD(Property = "Clips", Serialize)
    Array<MotionDatabaseClip> m_clips;

    HYP_FIELD(Property = "Weights", Serialize)
    MotionFeatureWeights m_weights;

    HYP_FIELD(Property = "TrajectoryHorizon", Serialize)
    float m_trajectoryHorizon = 1.0f;

    HYP_FIELD(Property = "SampleRate", Serialize)
    float m_sampleRate = 30.0f;

    HYP_FIELD(Property = "Frames", Serialize, Editor = false)
    Array<MotionDatabaseFrame> m_frames;

    HYP_FIELD(Property = "Features", Serialize, Editor = false)
    Array<float> m_features;

    // per feature group, the spread of its values across the database
    HYP_FIELD(Property = "FeatureSpreads", Serialize, Editor = false)
    Array<float> m_featureSpreads;

    Array<float> m_scales;
};

HYP_CLASS()
class ANIMATION_API AnimationMotionMatchingNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationMotionMatchingNode);

public:
    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    // "Walk 0.42s"
    String DescribeState(const AnimationGraphNodeState& state) const;

    HYP_FIELD(Property = "Database", Serialize = true, Editor = true)
    Handle<MotionDatabase> database;

    HYP_FIELD(Property = "SearchInterval", Serialize = true, Editor = true)
    float searchInterval = 0.1f;

    HYP_FIELD(Property = "BlendTime", Serialize = true, Editor = true)
    float blendTime = 0.2f;

    // a better match in the clip already playing must be at least this far away in time to jump to it
    HYP_FIELD(Property = "SameClipWindow", Serialize = true, Editor = true)
    float sameClipWindow = 0.2f;

    HYP_FIELD(Property = "VelocityParameter", Serialize = true, Editor = true)
    Name velocityParameter = NAME("Velocity");

    HYP_FIELD(Property = "DesiredVelocityParameter", Serialize = true, Editor = true)
    Name desiredVelocityParameter = NAME("DesiredVelocity");

    HYP_FIELD(Property = "DesiredFacingParameter", Serialize = true, Editor = true)
    Name desiredFacingParameter = NAME("DesiredFacing");

    HYP_FIELD(Property = "AccelerationParameter", Serialize = true, Editor = true)
    Name accelerationParameter = NAME("TrajectoryAcceleration");

    HYP_FIELD(Property = "TurnSharpnessParameter", Serialize = true, Editor = true)
    Name turnSharpnessParameter = NAME("TrajectoryTurnSharpness");

    HYP_FIELD(Property = "Acceleration", Serialize = true, Editor = true)
    float acceleration = 12.0f;

    HYP_FIELD(Property = "TurnSharpness", Serialize = true, Editor = true)
    float turnSharpness = 10.0f;

private:
    void Search(AnimationGraphContext& context, AnimationGraphNodeState& state) const;
};

} // namespace Hyperion
