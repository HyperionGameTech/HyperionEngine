/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/MotionMatching.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/HumanoidMapping.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Resource/ResLock.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <MotionMatching.generated.inl>

namespace Hyperion {

namespace {

static constexpr float VelocitySampleTime = 1.0f / 60.0f;

static constexpr uint32 FeatureGroupStarts[] = {
    uint32(MotionFeature::LeftFootPosition),
    uint32(MotionFeature::LeftFootVelocity),
    uint32(MotionFeature::HipVelocity),
    uint32(MotionFeature::TrajectoryPosition),
    uint32(MotionFeature::TrajectoryFacing),
    uint32(MotionFeature::Count)
};

static constexpr uint32 NumFeatureGroups = uint32(sizeof(FeatureGroupStarts) / sizeof(FeatureGroupStarts[0])) - 1;

float GetGroupWeight(const MotionFeatureWeights& weights, uint32 group)
{
    switch (group)
    {
    case 0:
        return weights.footPosition;
    case 1:
        return weights.footVelocity;
    case 2:
        return weights.hipVelocity;
    case 3:
        return weights.trajectoryPosition;
    default:
        return weights.trajectoryFacing;
    }
}

// where a clip's character stands on the ground and which way it faces, in model space
struct CharacterFrame
{
    Vec3f position;
    Vec3f facing;
};

// samples one clip of the database as if it played on and on, carrying its movement across loops
class ClipSampler
{
public:
    ClipSampler(const MotionDatabaseClip& entry, const Skeleton& skeleton, const Vec3f& up, const Vec3f& forward)
        : m_clip(*entry.clip),
          m_bindData(skeleton.GetBindData()),
          m_binding(PoseOps::BindClip(*entry.clip, skeleton)),
          m_up(up),
          m_forward(forward),
          m_length(entry.clip->GetLength()),
          m_isLooping(entry.loop)
    {
        const Vec3f right = up.Cross(forward);

        m_velocity = right * entry.velocity.x + forward * entry.velocity.y;

        m_pose.Resize(m_bindData.NumSlots());
        m_modelMatrices.Resize(m_bindData.NumSlots());
        m_skinningMatrices.Resize(m_bindData.NumSlots());

        if (m_clip.HasRootMotion())
        {
            m_rootSlot = m_binding.rootSlot;
        }

        if (HasRootMotion())
        {
            SampleModel(0.0f);

            m_inverseRootBindRotation = m_modelMatrices[m_rootSlot].ExtractRotation().Inverse();
            m_rootStart = Flatten(m_modelMatrices[m_rootSlot].ExtractTranslation());

            SampleClipAt(m_length);

            m_loopDisplacement = Flatten(m_modelMatrices[m_rootSlot].ExtractTranslation()) - m_rootStart;
        }
    }

    bool HasRootMotion() const
    {
        return m_rootSlot != SkeletonBindData::InvalidSlot;
    }

    float GetLength() const
    {
        return m_length;
    }

    bool IsLooping() const
    {
        return m_isLooping;
    }

    CharacterFrame GetCharacter(float time)
    {
        if (!HasRootMotion())
        {
            return CharacterFrame { Flatten(m_velocity * time), m_forward };
        }

        const float cycles = SampleModel(time);

        CharacterFrame frame;
        frame.position = Flatten(m_modelMatrices[m_rootSlot].ExtractTranslation()) + m_loopDisplacement * cycles;
        frame.facing = Flatten((m_modelMatrices[m_rootSlot].ExtractRotation() * m_inverseRootBindRotation).RotateVector(m_forward));
        frame.facing = frame.facing.LengthSquared() > 1e-10f ? frame.facing.Normalized() : m_forward;

        return frame;
    }

    Vec3f GetBonePosition(uint32 slot, float time)
    {
        const float cycles = SampleModel(time);
        const Vec3f position = m_modelMatrices[slot].ExtractTranslation();

        if (HasRootMotion())
        {
            return position + m_loopDisplacement * cycles;
        }

        return position + m_velocity * time;
    }

private:
    Vec3f Flatten(const Vec3f& direction) const
    {
        return direction - m_up * direction.Dot(m_up);
    }

    // poses the model at time, returning how many whole loops came before it
    float SampleModel(float time)
    {
        float cycles = 0.0f;
        float clipTime = MathUtil::Clamp(time, 0.0f, m_length);

        if (m_isLooping && m_length > 0.0f)
        {
            cycles = std::floor(time / m_length);
            clipTime = time - cycles * m_length;
        }

        SampleClipAt(clipTime);

        return cycles;
    }

    void SampleClipAt(float clipTime)
    {
        for (uint32 slot = 0; slot < m_bindData.NumSlots(); ++slot)
        {
            m_pose[slot] = m_bindData.restLocals[slot];
        }

        PoseOps::SampleClip(m_clip, m_binding, m_bindData, clipTime, m_pose.ToSpan());
        PoseOps::ComputeMatrices(m_bindData, m_pose.ToSpan(), m_modelMatrices.ToSpan(), m_skinningMatrices.ToSpan());
    }

    const Animation& m_clip;
    const SkeletonBindData& m_bindData;
    ClipBinding m_binding;
    Vec3f m_up;
    Vec3f m_forward;
    Vec3f m_velocity;
    float m_length;
    bool m_isLooping;

    uint32 m_rootSlot = SkeletonBindData::InvalidSlot;
    Quat4f m_inverseRootBindRotation;
    Vec3f m_rootStart;
    Vec3f m_loopDisplacement;

    Array<Transform> m_pose;
    Array<Mat4f> m_modelMatrices;
    Array<Mat4f> m_skinningMatrices;
};

} // namespace

#pragma region MotionDatabase

MotionDatabase::MotionDatabase() = default;

MotionDatabase::MotionDatabase(Name name)
{
    SetName(name);
}

MotionDatabase::~MotionDatabase() = default;

void MotionDatabase::OnLoaded()
{
    UpdateScales();
}

void MotionDatabase::ClearBuild()
{
    m_frames.Clear();
    m_features.Clear();
    m_featureSpreads.Clear();
    m_scales.Clear();
}

bool MotionDatabase::Build()
{
    ClearBuild();

    if (!m_skeleton.IsValid())
    {
        HYP_LOG(Animation, Warning, "Motion database '{}' has no skeleton to build against", GetName());

        return false;
    }

    const Skeleton& skeleton = *m_skeleton;
    const SkeletonBindData& bindData = skeleton.GetBindData();
    const HumanoidMapping& mapping = skeleton.GetHumanoidMapping();

    const Vec3f up = PoseOps::GetModelUp(skeleton);
    const Vec3f forward = PoseOps::GetModelForward(skeleton);

    uint32 leftFoot = SkeletonBindData::InvalidSlot;
    uint32 rightFoot = SkeletonBindData::InvalidSlot;
    uint32 hips = SkeletonBindData::InvalidSlot;

    if (skeleton.IsHumanoid())
    {
        leftFoot = bindData.FindSlot(mapping.GetBoneName(HumanoidBone::LeftFoot));
        rightFoot = bindData.FindSlot(mapping.GetBoneName(HumanoidBone::RightFoot));
        hips = bindData.FindSlot(mapping.GetBoneName(HumanoidBone::Hips));
    }

    const bool hasPoseFeatures = leftFoot != SkeletonBindData::InvalidSlot
        && rightFoot != SkeletonBindData::InvalidSlot
        && hips != SkeletonBindData::InvalidSlot;

    if (!hasPoseFeatures)
    {
        HYP_LOG(Animation, Warning, "Motion database '{}': '{}' isn't humanoid, so only the trajectory is matched", GetName(), skeleton.GetName());
    }

    // the tracks' keyframes stay loaded only while something holds them
    Array<UniquePtr<TSharedResLock<AssetObject>>> trackLocks;

    for (const MotionDatabaseClip& entry : m_clips)
    {
        if (!entry.clip.IsValid())
        {
            continue;
        }

        for (const Handle<AnimationTrack>& track : entry.clip->GetTracks())
        {
            if (track.IsValid())
            {
                trackLocks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
            }
        }
    }

    const float frameStep = 1.0f / MathUtil::Max(m_sampleRate, 1.0f);

    for (uint32 clipIndex = 0; clipIndex < m_clips.Size(); ++clipIndex)
    {
        const MotionDatabaseClip& entry = m_clips[clipIndex];

        if (!entry.clip.IsValid() || entry.clip->GetLength() <= 0.0f)
        {
            continue;
        }

        ClipSampler sampler(entry, skeleton, up, forward);

        const float length = sampler.GetLength();

        // a clip that ends needs its whole trajectory inside it
        const float lastTime = sampler.IsLooping() ? length - frameStep * 0.5f : length - m_trajectoryHorizon;

        if (lastTime < 0.0f)
        {
            HYP_LOG(Animation, Warning, "Motion database '{}': '{}' is shorter than the trajectory and doesn't loop, so it's left out", GetName(), entry.clip->GetName());

            continue;
        }

        for (float time = 0.0f; time <= lastTime; time += frameStep)
        {
            const CharacterFrame now = sampler.GetCharacter(time);
            const Vec3f nowRight = up.Cross(now.facing);

            const auto toCharacter = [&](const Vec3f& vector)
            {
                return Vec3f(vector.Dot(nowRight), vector.Dot(up), vector.Dot(now.facing));
            };

            float features[NumFeatures] = {};

            const auto write = [&features](MotionFeature feature, uint32 offset, const Vec3f& value)
            {
                const uint32 index = uint32(feature) + offset;

                features[index] = value.x;
                features[index + 1] = value.y;
                features[index + 2] = value.z;
            };

            if (hasPoseFeatures)
            {
                float before = time - VelocitySampleTime;
                float after = time + VelocitySampleTime;

                if (!sampler.IsLooping())
                {
                    before = MathUtil::Max(before, 0.0f);
                    after = MathUtil::Min(after, length);
                }

                const auto velocityOf = [&](uint32 slot)
                {
                    return (sampler.GetBonePosition(slot, after) - sampler.GetBonePosition(slot, before)) / MathUtil::Max(after - before, 1e-6f);
                };

                write(MotionFeature::LeftFootPosition, 0, toCharacter(sampler.GetBonePosition(leftFoot, time) - now.position));
                write(MotionFeature::RightFootPosition, 0, toCharacter(sampler.GetBonePosition(rightFoot, time) - now.position));
                write(MotionFeature::LeftFootVelocity, 0, toCharacter(velocityOf(leftFoot)));
                write(MotionFeature::RightFootVelocity, 0, toCharacter(velocityOf(rightFoot)));
                write(MotionFeature::HipVelocity, 0, toCharacter(velocityOf(hips)));
            }

            for (uint32 point = 0; point < NumTrajectoryPoints; ++point)
            {
                const CharacterFrame future = sampler.GetCharacter(time + GetTrajectoryTime(point));

                const Vec3f offset = toCharacter(future.position - now.position);
                const Vec3f facing = toCharacter(future.facing);

                features[uint32(MotionFeature::TrajectoryPosition) + point * 2] = offset.x;
                features[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1] = offset.z;
                features[uint32(MotionFeature::TrajectoryFacing) + point * 2] = facing.x;
                features[uint32(MotionFeature::TrajectoryFacing) + point * 2 + 1] = facing.z;
            }

            m_frames.PushBack(MotionDatabaseFrame { clipIndex, time });

            for (float feature : features)
            {
                m_features.PushBack(feature);
            }
        }
    }

    if (m_frames.Empty())
    {
        HYP_LOG(Animation, Warning, "Motion database '{}' has no frames", GetName());

        return false;
    }

    // each group is scaled by how much it varies, so the weights compare like with like
    m_featureSpreads.Resize(NumFeatureGroups);

    for (uint32 group = 0; group < NumFeatureGroups; ++group)
    {
        double variance = 0.0;

        for (uint32 feature = FeatureGroupStarts[group]; feature < FeatureGroupStarts[group + 1]; ++feature)
        {
            double mean = 0.0;

            for (uint32 frame = 0; frame < m_frames.Size(); ++frame)
            {
                mean += m_features[size_t(frame) * NumFeatures + feature];
            }

            mean /= double(m_frames.Size());

            for (uint32 frame = 0; frame < m_frames.Size(); ++frame)
            {
                const double difference = m_features[size_t(frame) * NumFeatures + feature] - mean;

                variance += difference * difference;
            }
        }

        variance /= double(m_frames.Size()) * double(FeatureGroupStarts[group + 1] - FeatureGroupStarts[group]);

        m_featureSpreads[group] = float(std::sqrt(variance));
    }

    UpdateScales();

    HYP_LOG(Animation, Info, "Motion database '{}' built: {} frames from {} clips", GetName(), m_frames.Size(), m_clips.Size());

    return true;
}

void MotionDatabase::UpdateScales()
{
    m_scales.Clear();

    if (m_featureSpreads.Size() != NumFeatureGroups)
    {
        return;
    }

    m_scales.Resize(NumFeatures);

    for (uint32 group = 0; group < NumFeatureGroups; ++group)
    {
        const float spread = m_featureSpreads[group];
        const float scale = spread > 1e-6f ? GetGroupWeight(m_weights, group) / spread : 0.0f;

        for (uint32 feature = FeatureGroupStarts[group]; feature < FeatureGroupStarts[group + 1]; ++feature)
        {
            m_scales[feature] = scale;
        }
    }
}

uint32 MotionDatabase::FindFrame(uint32 clipIndex, float time) const
{
    uint32 nearest = ~0u;
    float nearestDistance = MathUtil::MaxSafeValue<float>();

    for (uint32 frame = 0; frame < m_frames.Size(); ++frame)
    {
        if (m_frames[frame].clipIndex != clipIndex)
        {
            continue;
        }

        const float distance = MathUtil::Abs(m_frames[frame].time - time);

        if (distance < nearestDistance)
        {
            nearest = frame;
            nearestDistance = distance;
        }
    }

    return nearest;
}

uint32 MotionDatabase::Search(Span<const float> query, bool includePose) const
{
    Assert(IsBuilt() && m_scales.Size() == NumFeatures, "Motion database '{}' searched before it was built", GetName());
    Assert(query.Size() == NumFeatures);

    const uint32 firstFeature = includePose ? 0 : uint32(MotionFeature::TrajectoryPosition);

    uint32 best = ~0u;
    float bestCost = MathUtil::MaxSafeValue<float>();

    for (uint32 frame = 0; frame < m_frames.Size(); ++frame)
    {
        const float* features = m_features.Data() + size_t(frame) * NumFeatures;

        float cost = 0.0f;

        for (uint32 feature = firstFeature; feature < NumFeatures && cost < bestCost; ++feature)
        {
            const float difference = (query[feature] - features[feature]) * m_scales[feature];

            cost += difference * difference;
        }

        if (cost < bestCost)
        {
            best = frame;
            bestCost = cost;
        }
    }

    return best;
}

Vec2f MotionDatabase::PredictPosition(const Vec2f& velocity, const Vec2f& desiredVelocity, float acceleration, float time)
{
    if (acceleration <= 1e-4f)
    {
        return velocity * time;
    }

    return desiredVelocity * time + (velocity - desiredVelocity) * ((1.0f - MathUtil::Exp(-acceleration * time)) / acceleration);
}

#pragma endregion MotionDatabase

#pragma region AnimationMotionMatchingNode

void AnimationMotionMatchingNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    AnimationGraphNodeState& state = context.GetState(nodeIndex);
    state.weight += weight;

    if (!database.IsValid())
    {
        return;
    }

    if (!database->IsBuilt())
    {
        HYP_LOG_ONCE(Animation, Error, "Motion database '{}' isn't built, so its motion matching node plays nothing", database->GetName());

        return;
    }

    const Array<MotionDatabaseClip>& clips = database->GetClips();

    if (context.BeginNodeUpdate(nodeIndex))
    {
        const float delta = context.GetDelta();

        if (!state.wasActive || state.currentState >= clips.Size() || !clips[state.currentState].clip.IsValid())
        {
            state.currentState = InvalidAnimationGraphNode;
            state.isInertializing = false;
            state.isInertializePending = false;
        }

        bool mustSearch = state.currentState == InvalidAnimationGraphNode;

        state.previousTime = state.time;

        if (state.currentState != InvalidAnimationGraphNode)
        {
            const MotionDatabaseClip& entry = clips[state.currentState];
            const float length = entry.clip->GetLength();

            state.time += delta;

            if (state.time >= length)
            {
                if (entry.loop && length > 0.0f)
                {
                    state.time = std::fmod(state.time, length);
                }
                else
                {
                    state.time = length;
                    mustSearch = true;
                }
            }
        }

        state.stateTime += delta;
        state.transitionTime += delta;

        if (state.isInertializing && state.transitionTime >= state.transitionDuration)
        {
            state.isInertializing = false;
        }

        if (mustSearch || state.stateTime >= searchInterval)
        {
            state.stateTime = 0.0f;

            Search(context, state);
        }
    }

    if (state.currentState == InvalidAnimationGraphNode)
    {
        return;
    }

    const Animation& clip = *clips[state.currentState].clip;

    // time only runs forward here and a search jump starts previousTime over, so going back means the clip looped
    if (clip.HasRootMotion())
    {
        context.AddRootMotion(PoseOps::GetRootMotion(clip, context.GetClipBinding(clip), context.GetModelUp(), state.previousTime, state.time, state.time < state.previousTime), weight);
    }
}

void AnimationMotionMatchingNode::Search(AnimationGraphContext& context, AnimationGraphNodeState& state) const
{
    const MotionDatabase& motionDatabase = *database;

    float query[MotionDatabase::NumFeatures] = {};

    const uint32 currentFrame = state.currentState != InvalidAnimationGraphNode
        ? motionDatabase.FindFrame(state.currentState, state.time)
        : ~0u;

    // the pose part of the query is where the playing clip already is
    if (currentFrame != ~0u)
    {
        const Span<const float> currentFeatures = motionDatabase.GetFrameFeatures(currentFrame);

        for (uint32 feature = 0; feature < uint32(MotionFeature::TrajectoryPosition); ++feature)
        {
            query[feature] = currentFeatures[feature];
        }
    }

    const Vec2f velocity = context.GetVector2(velocityParameter);
    const Vec2f desiredVelocity = context.GetVector2(desiredVelocityParameter);
    const float desiredFacing = MathUtil::DegToRad(context.GetFloat(desiredFacingParameter));
    const float trajectoryAcceleration = context.GetFloat(accelerationParameter, acceleration);
    const float trajectoryTurnSharpness = context.GetFloat(turnSharpnessParameter, turnSharpness);

    for (uint32 point = 0; point < MotionDatabase::NumTrajectoryPoints; ++point)
    {
        const float time = motionDatabase.GetTrajectoryTime(point);

        const Vec2f position = MotionDatabase::PredictPosition(velocity, desiredVelocity, trajectoryAcceleration, time);
        const float facingAngle = desiredFacing * (1.0f - MathUtil::Exp(-trajectoryTurnSharpness * time));

        query[uint32(MotionFeature::TrajectoryPosition) + point * 2] = position.x;
        query[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1] = position.y;
        query[uint32(MotionFeature::TrajectoryFacing) + point * 2] = std::sin(facingAngle);
        query[uint32(MotionFeature::TrajectoryFacing) + point * 2 + 1] = std::cos(facingAngle);
    }

    const uint32 best = motionDatabase.Search(Span<const float>(query, MotionDatabase::NumFeatures), currentFrame != ~0u);

    if (best == ~0u)
    {
        return;
    }

    const MotionDatabaseFrame& frame = motionDatabase.GetFrame(best);

    if (frame.clipIndex == state.currentState)
    {
        const MotionDatabaseClip& entry = motionDatabase.GetClips()[frame.clipIndex];

        float distance = MathUtil::Abs(frame.time - state.time);

        if (entry.loop)
        {
            distance = MathUtil::Min(distance, entry.clip->GetLength() - distance);
        }

        // already playing close enough to the best match
        if (distance < sameClipWindow)
        {
            return;
        }
    }

    const bool hadClip = state.currentState != InvalidAnimationGraphNode;

    state.currentState = frame.clipIndex;
    state.time = frame.time;
    state.previousTime = frame.time;

    if (hadClip && blendTime > 0.0f)
    {
        state.isInertializing = true;
        state.isInertializePending = true;
        state.transitionTime = 0.0f;
        state.transitionDuration = blendTime;
    }
}

void AnimationMotionMatchingNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    AnimationGraphNodeState& state = context.GetState(nodeIndex);

    const MotionDatabaseClip* entry = nullptr;

    if (database.IsValid() && state.currentState < database->GetClips().Size())
    {
        entry = &database->GetClips()[state.currentState];
    }

    if (!entry || !entry->clip.IsValid())
    {
        context.EvaluateInput(InvalidAnimationGraphNode, outPose);

        return;
    }

    const Animation& clip = *entry->clip;
    const ClipBinding& binding = context.GetClipBinding(clip);

    PoseOps::SampleClip(clip, binding, context.GetBindData(), state.time, outPose);

    if (clip.HasRootMotion())
    {
        PoseOps::RemoveRootMotion(clip, binding, context.GetModelUp(), state.time, outPose);
    }

    context.ApplyInertialization(nodeIndex, outPose);
}

String AnimationMotionMatchingNode::DescribeState(const AnimationGraphNodeState& state) const
{
    if (!database.IsValid() || state.currentState >= database->GetClips().Size() || !database->GetClips()[state.currentState].clip.IsValid())
    {
        return "(no match)";
    }

    return HYP_FORMAT("{} {}s", database->GetClips()[state.currentState].clip->GetName(), state.time);
}

#pragma endregion AnimationMotionMatchingNode

} // namespace Hyperion
