/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

class Animation;
class BoneMask;
class AnimationGraph;
class AnimationGraphInstance;

HYP_ENUM()
enum class AnimationPlaybackStatus : uint8
{
    Stopped = 0,
    Paused,
    Playing
};

HYP_ENUM()
enum class AnimationLoopMode : uint8
{
    Once = 0,
    Repeat
};

HYP_STRUCT()
struct AnimationPlaybackState
{
    HYP_STRUCT_BODY(AnimationPlaybackState);

    HYP_FIELD(Property = "AnimationIndex", Editor = false)
    uint32 animationIndex = ~0u;

    HYP_FIELD(Property = "Status", Editor = false)
    AnimationPlaybackStatus status = AnimationPlaybackStatus::Stopped;

    HYP_FIELD(Property = "LoopMode", Editor = true)
    AnimationLoopMode loopMode = AnimationLoopMode::Once;

    HYP_FIELD(Property = "Speed", Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Editor = false)
    float currentTime = 0.0f;

    /// second animation mixed on top of the first by layerWeight
    HYP_FIELD(Property = "LayerAnimationIndex", Editor = false)
    uint32 layerAnimationIndex = ~0u;

    HYP_FIELD(Property = "LayerTime", Editor = false)
    float layerTime = 0.0f;

    HYP_FIELD(Property = "LayerWeight", Editor = false)
    float layerWeight = 0.0f;

    /// if set, the layer leaves this bone and everything beneath it to the base animation
    HYP_FIELD(Property = "LayerExcludedBone", Editor = false, EditorType = "Bone")
    Name layerExcludedBone;

    HYP_FIELD(Property = "SecondLayerAnimationIndex", Editor = false)
    uint32 secondLayerAnimationIndex = ~0u;

    HYP_FIELD(Property = "SecondLayerTime", Editor = false)
    float secondLayerTime = 0.0f;

    HYP_FIELD(Property = "SecondLayerWeight", Editor = false)
    float secondLayerWeight = 0.0f;

    HYP_FIELD(Property = "TwistAngle", Editor = false)
    float twistAngle = 0.0f;

    HYP_FIELD(Property = "TwistRootBone", Editor = false, EditorType = "Bone")
    Name twistRootBone;

    HYP_FIELD(Property = "TwistEndBone", Editor = false, EditorType = "Bone")
    Name twistEndBone;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(animationIndex);
        hc.Add(status);
        hc.Add(loopMode);
        hc.Add(speed);
        hc.Add(currentTime);
        hc.Add(layerAnimationIndex);
        hc.Add(layerTime);
        hc.Add(layerWeight);
        hc.Add(layerExcludedBone);
        hc.Add(secondLayerAnimationIndex);
        hc.Add(secondLayerTime);
        hc.Add(secondLayerWeight);
        hc.Add(twistAngle);
        hc.Add(twistRootBone);
        hc.Add(twistEndBone);

        return hc;
    }
};

/*! \brief A component that holds animation play state for a given entity. */
HYP_STRUCT(Component, Label = "Animation Component", Editor = true)
struct ANIMATION_API AnimationComponent
{
    HYP_STRUCT_BODY(AnimationComponent);

    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AnimationPlaybackState playbackState;

    HYP_FIELD(Property = "Animation", Serialize = true, Editor = true, EditorType = "AnimationClip")
    Handle<Animation> animation;

    HYP_FIELD(Property = "PlayOnStart", Serialize = true, Editor = true)
    bool playOnStart = true;

    HYP_FIELD(Property = "LayerMask", Serialize = true, Editor = false)
    Handle<BoneMask> layerMask;

    // when set, the graph drives the pose and playbackState is ignored
    HYP_FIELD(Property = "Graph", Serialize = true, Editor = true)
    Handle<AnimationGraph> graph;

    HYP_FIELD(Transient, Editor = false)
    Handle<AnimationGraphInstance> graphInstance;

    HYP_FIELD(Transient, Editor = false)
    float crossFadeRemaining = 0.0f;

    void Play(const Handle<Animation>& clip, AnimationLoopMode loopMode = AnimationLoopMode::Repeat, float speed = 1.0f);
    void CrossFade(const Handle<Animation>& clip, float duration, AnimationLoopMode loopMode = AnimationLoopMode::Repeat, float speed = 1.0f);
    void Stop();

    HYP_FORCE_INLINE bool IsPlaying() const
    {
        return playbackState.status == AnimationPlaybackStatus::Playing;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(playbackState);
        hc.Add(animation);
        hc.Add(playOnStart);
        hc.Add(layerMask);
        hc.Add(graph);

        return hc;
    }
};

} // namespace Hyperion
