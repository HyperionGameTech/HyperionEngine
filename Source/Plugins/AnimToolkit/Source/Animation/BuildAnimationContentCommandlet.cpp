#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Animation/AnimationGraph.hpp>
#include <Animation/MotionMatching.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Core.hpp>

namespace Hyperion {

#ifdef HYP_EDITOR

namespace {

// the speeds the walk and run clips were recorded at; CharacterModelComponent::walkReferenceSpeed matches the walk
constexpr float LocomotionWalkSpeed = 1.02f;
constexpr float LocomotionRunSpeed = 8.6f;

// how long a character is off the ground before it counts as falling rather than stepping down
constexpr float LocomotionFallGraceTime = 0.12f;

// a landing while moving hands back to locomotion over this fraction of the land clip
constexpr float LocomotionMovingLandFraction = 0.35f;

void BuildHumanoidLocomotionGraph(const Handle<AssetRegistry>& clipRegistry, const Handle<AssetRegistry>& outputRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("HumanoidLocomotion"));

    bool hasAllClips = true;

    const auto addClip = [&](Name nodeName, Name clipName, Vec2f position, bool loop, Name syncGroup = Name::Invalid(), float referenceSpeed = 0.0f, Name timeParameter = Name::Invalid())
    {
        Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
        clipNode->name = nodeName;
        clipNode->clip = clipRegistry->GetAsset<Animation>(AssetBuckets::Animations, clipName);
        clipNode->loop = loop;
        clipNode->syncGroup = syncGroup;
        clipNode->editorPosition = position;

        if (referenceSpeed > 0.0f)
        {
            clipNode->speedParameter = NAME("Speed");
            clipNode->referenceSpeed = referenceSpeed;
        }

        if (timeParameter.IsValid())
        {
            clipNode->timeParameter = timeParameter;
            clipNode->normalizedTime = true;
        }

        if (!clipNode->clip.IsValid())
        {
            HYP_LOG(Engine, Error, "HumanoidLocomotion: the engine has no {} clip; build the character part first", clipName);
            hasAllClips = false;
        }

        return graph->AddNode(clipNode);
    };

    const auto addBlend1D = [&](Name nodeName, Name parameter, Vec2f position, bool smooth, Array<AnimationBlendSample> samples)
    {
        Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
        blendNode->name = nodeName;
        blendNode->parameter = parameter;
        blendNode->smooth = smooth;
        blendNode->samples = std::move(samples);
        blendNode->editorPosition = position;

        return graph->AddNode(blendNode);
    };

    const auto sample = [](uint32 input, float position)
    {
        return AnimationBlendSample { input, Vec2f(position, 0.0f) };
    };

    const auto condition = [](Name parameter, AnimationConditionOp op, float value = 0.0f)
    {
        return AnimationTransitionCondition { parameter, op, value };
    };

    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("MoveAngle"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("Windup"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("Airborne"), AnimationGraphParameterType::Bool);
    graph->AddParameter(NAME("Jumping"), AnimationGraphParameterType::Bool);
    graph->AddParameter(NAME("RunningJump"), AnimationGraphParameterType::Bool);
    graph->AddParameter(NAME("AirTime"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("TurnStep"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("TurnStepKind"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("TurnStepProgress"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("AimTwist"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("LeftFootOffset"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("RightFootOffset"), AnimationGraphParameterType::Float);

    // walking and running share one phase, each clip played at the rate that matches its stride to the ground speed
    const uint32 idle = addClip(NAME("Idle"), NAME("Idle"), Vec2f(0.0f, 0.0f), true);
    const uint32 walk = addClip(NAME("Walk"), NAME("Walk"), Vec2f(0.0f, 60.0f), true, NAME("Locomotion"), LocomotionWalkSpeed);
    const uint32 strafeLeft = addClip(NAME("StrafeLeft"), NAME("StrafeLeft"), Vec2f(0.0f, 120.0f), true, NAME("Locomotion"), LocomotionWalkSpeed);
    const uint32 strafeRight = addClip(NAME("StrafeRight"), NAME("StrafeRight"), Vec2f(0.0f, 180.0f), true, NAME("Locomotion"), LocomotionWalkSpeed);
    const uint32 walkBackward = addClip(NAME("WalkBackward"), NAME("WalkBackward"), Vec2f(0.0f, 240.0f), true, NAME("Locomotion"), LocomotionWalkSpeed);
    const uint32 run = addClip(NAME("Run"), NAME("Run"), Vec2f(0.0f, 300.0f), true, NAME("Locomotion"), LocomotionRunSpeed);

    // MoveAngle is in degrees from the body's forward, positive to the right
    const uint32 directional = addBlend1D(NAME("Directional"), NAME("MoveAngle"), Vec2f(250.0f, 120.0f), false,
        { sample(walkBackward, -180.0f), sample(strafeLeft, -90.0f), sample(walk, 0.0f), sample(strafeRight, 90.0f), sample(walkBackward, 180.0f) });

    const uint32 locomotion = addBlend1D(NAME("Locomotion"), NAME("Speed"), Vec2f(500.0f, 60.0f), true,
        { sample(idle, 0.0f), sample(directional, LocomotionWalkSpeed), sample(run, LocomotionRunSpeed) });

    // the body's turn steps: CharacterModelSystem decides them and drives their progress, since they turn the body
    const uint32 turnLeft = addClip(NAME("TurnLeft"), NAME("TurnLeft"), Vec2f(0.0f, 380.0f), false, Name::Invalid(), 0.0f, NAME("TurnStepProgress"));
    const uint32 turnRight = addClip(NAME("TurnRight"), NAME("TurnRight"), Vec2f(0.0f, 440.0f), false, Name::Invalid(), 0.0f, NAME("TurnStepProgress"));
    const uint32 turnLeftSmall = addClip(NAME("TurnLeftSmall"), NAME("TurnLeftSmall"), Vec2f(0.0f, 500.0f), false, Name::Invalid(), 0.0f, NAME("TurnStepProgress"));
    const uint32 turnRightSmall = addClip(NAME("TurnRightSmall"), NAME("TurnRightSmall"), Vec2f(0.0f, 560.0f), false, Name::Invalid(), 0.0f, NAME("TurnStepProgress"));

    const uint32 turnSteps = addBlend1D(NAME("TurnSteps"), NAME("TurnStepKind"), Vec2f(250.0f, 440.0f), false,
        { sample(turnLeft, 0.0f), sample(turnRight, 1.0f), sample(turnLeftSmall, 2.0f), sample(turnRightSmall, 3.0f) });

    Handle<AnimationLayerNode> standingNode = MakeHandle<AnimationLayerNode>();
    standingNode->name = NAME("Standing");
    standingNode->base = locomotion;
    standingNode->layer = turnSteps;
    standingNode->weightParameter = NAME("TurnStep");
    standingNode->editorPosition = Vec2f(750.0f, 200.0f);

    const uint32 standing = graph->AddNode(standingNode);

    const uint32 windupClip = addClip(NAME("JumpWindup"), NAME("JumpWindup"), Vec2f(500.0f, 380.0f), false, Name::Invalid(), 0.0f, NAME("Windup"));

    const uint32 ground = addBlend1D(NAME("Ground"), NAME("Windup"), Vec2f(1000.0f, 200.0f), true,
        { sample(standing, 0.0f), sample(windupClip, 1.0f) });

    // a running takeoff (RunningJump, fixed for the whole jump) uses the running variants
    const uint32 jumpClip = addClip(NAME("Jump"), NAME("Jump"), Vec2f(750.0f, 640.0f), false);
    const uint32 runJumpClip = addClip(NAME("RunJump"), NAME("RunJump"), Vec2f(750.0f, 700.0f), false);
    const uint32 fallClip = addClip(NAME("Fall"), NAME("Fall"), Vec2f(750.0f, 780.0f), true);
    const uint32 runFallClip = addClip(NAME("RunFall"), NAME("RunFall"), Vec2f(750.0f, 840.0f), true);
    const uint32 landClip = addClip(NAME("Land"), NAME("Land"), Vec2f(750.0f, 920.0f), false);
    const uint32 runLandClip = addClip(NAME("RunLand"), NAME("RunLand"), Vec2f(750.0f, 980.0f), false);

    const uint32 jump = addBlend1D(NAME("JumpVariant"), NAME("RunningJump"), Vec2f(1000.0f, 670.0f), false, { sample(jumpClip, 0.0f), sample(runJumpClip, 1.0f) });
    const uint32 fall = addBlend1D(NAME("FallVariant"), NAME("RunningJump"), Vec2f(1000.0f, 810.0f), false, { sample(fallClip, 0.0f), sample(runFallClip, 1.0f) });
    const uint32 land = addBlend1D(NAME("LandVariant"), NAME("RunningJump"), Vec2f(1000.0f, 950.0f), false, { sample(landClip, 0.0f), sample(runLandClip, 1.0f) });

    if (!hasAllClips)
    {
        return;
    }

    // a clip's length comes from its track data, which is only there while the tracks are held resident
    const auto clipLength = [&graph](uint32 nodeIndex)
    {
        const Animation& clip = *static_cast<const AnimationClipNode*>(graph->GetNode(nodeIndex))->clip;

        Array<UniquePtr<TSharedResLock<AssetObject>>> trackLocks;

        for (const Handle<AnimationTrack>& track : clip.GetTracks())
        {
            if (track.IsValid())
            {
                trackLocks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
            }
        }

        return clip.GetLength();
    };

    const float jumpLength = clipLength(jumpClip);
    const float landLength = clipLength(landClip);

    const auto transition = [](uint32 from, uint32 to, float duration, Array<AnimationTransitionCondition> conditions, int32 priority = 0)
    {
        AnimationStateTransition stateTransition;
        stateTransition.from = from;
        stateTransition.to = to;
        stateTransition.duration = duration;
        stateTransition.curve = AnimationBlendCurve::SmoothStep;
        stateTransition.conditions = std::move(conditions);
        stateTransition.priority = priority;

        return stateTransition;
    };

    enum : uint32
    {
        GroundState = 0,
        JumpState,
        FallState,
        LandState
    };

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->name = NAME("Movement");
    stateMachine->editorPosition = Vec2f(1250.0f, 400.0f);
    stateMachine->states = {
        AnimationState { NAME("Ground"), ground, Vec2f(0.0f, 0.0f) },
        AnimationState { NAME("Jump"), jump, Vec2f(250.0f, -120.0f) },
        AnimationState { NAME("Fall"), fall, Vec2f(500.0f, 0.0f) },
        AnimationState { NAME("Land"), land, Vec2f(250.0f, 120.0f) }
    };

    AnimationStateTransition fromWindup = transition(GroundState, JumpState, 0.05f,
        { condition(NAME("Airborne"), AnimationConditionOp::IsTrue), condition(NAME("Jumping"), AnimationConditionOp::IsTrue), condition(NAME("Windup"), AnimationConditionOp::Greater, 0.5f) }, 1);

    AnimationStateTransition landToMoving = transition(LandState, GroundState, landLength * LocomotionMovingLandFraction,
        { condition(NAME("Speed"), AnimationConditionOp::Greater, LocomotionWalkSpeed * 0.5f) }, 1);
    landToMoving.interruptible = true;

    AnimationStateTransition landToStanding = transition(LandState, GroundState, landLength, {});
    landToStanding.interruptible = true;

    stateMachine->transitions = {
        fromWindup,
        transition(GroundState, JumpState, 0.25f, { condition(NAME("Airborne"), AnimationConditionOp::IsTrue), condition(NAME("Jumping"), AnimationConditionOp::IsTrue) }),
        transition(GroundState, FallState, 0.2f,
            { condition(NAME("Airborne"), AnimationConditionOp::IsTrue), condition(NAME("Jumping"), AnimationConditionOp::IsFalse), condition(NAME("AirTime"), AnimationConditionOp::Greater, LocomotionFallGraceTime) }),
        transition(JumpState, FallState, jumpLength * 0.4f, { condition(Name::Invalid(), AnimationConditionOp::StateNormalizedTimeAtLeast, 0.6f) }),
        transition(JumpState, LandState, 0.0f, { condition(NAME("Airborne"), AnimationConditionOp::IsFalse) }),
        transition(FallState, LandState, 0.0f, { condition(NAME("Airborne"), AnimationConditionOp::IsFalse) }),
        landToMoving,
        landToStanding
    };

    const uint32 movement = graph->AddNode(stateMachine);

    Handle<AnimationFootIKNode> footIKNode = MakeHandle<AnimationFootIKNode>();
    footIKNode->name = NAME("FootIK");
    footIKNode->input = movement;
    footIKNode->editorPosition = Vec2f(1500.0f, 400.0f);

    const uint32 footIK = graph->AddNode(footIKNode);

    Handle<AnimationChainTwistNode> aimNode = MakeHandle<AnimationChainTwistNode>();
    aimNode->name = NAME("Aim");
    aimNode->input = footIK;
    aimNode->rootBone = NAME("spine_01");
    aimNode->endBone = NAME("spine_03");
    aimNode->angleParameter = NAME("AimTwist");
    aimNode->editorPosition = Vec2f(1800.0f, 400.0f);

    graph->SetRootNode(graph->AddNode(aimNode));

    outputRegistry->PutAssetsDeep(graph, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "HumanoidLocomotion graph built ({} nodes).", graph->NumNodes());
}

void BuildHumanoidMotionMatching(const Handle<AssetRegistry>& clipRegistry, const Handle<AssetRegistry>& outputRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

    const Handle<Skeleton> skeleton = clipRegistry->GetAsset<Skeleton>(AssetBuckets::Skeletons, NAME("BaseHumanRig"));

    if (!skeleton.IsValid())
    {
        HYP_LOG(Engine, Error, "HumanoidMotion: the engine has no BaseHumanRig skeleton; build the character part first");

        return;
    }

    Array<MotionDatabaseClip> clips;

    // the engine's locomotion clips play in place, so each is given the ground speed its stride covers
    const auto addClip = [&](Name clipName, Vec2f velocity)
    {
        MotionDatabaseClip entry;
        entry.clip = clipRegistry->GetAsset<Animation>(AssetBuckets::Animations, clipName);
        entry.velocity = velocity;

        if (!entry.clip.IsValid())
        {
            HYP_LOG(Engine, Error, "HumanoidMotion: the engine has no {} clip; build the character part first", clipName);

            return;
        }

        clips.PushBack(entry);
    };

    addClip(NAME("Idle"), Vec2f::Zero());
    addClip(NAME("Walk"), Vec2f(0.0f, LocomotionWalkSpeed));
    addClip(NAME("StrafeLeft"), Vec2f(-LocomotionWalkSpeed, 0.0f));
    addClip(NAME("StrafeRight"), Vec2f(LocomotionWalkSpeed, 0.0f));
    addClip(NAME("WalkBackward"), Vec2f(0.0f, -LocomotionWalkSpeed));
    addClip(NAME("Run"), Vec2f(0.0f, LocomotionRunSpeed));

    Handle<MotionDatabase> database = MakeHandle<MotionDatabase>(NAME("HumanoidMotion"));
    database->SetSkeleton(skeleton);
    database->SetClips(clips);

    if (!database->Build())
    {
        return;
    }

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("HumanoidMotionMatching"));
    graph->AddParameter(NAME("Velocity"), AnimationGraphParameterType::Vector2);
    graph->AddParameter(NAME("DesiredVelocity"), AnimationGraphParameterType::Vector2);
    graph->AddParameter(NAME("DesiredFacing"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("TrajectoryAcceleration"), AnimationGraphParameterType::Float, Vec2f(12.0f, 0.0f));
    graph->AddParameter(NAME("TrajectoryTurnSharpness"), AnimationGraphParameterType::Float, Vec2f(10.0f, 0.0f));
    graph->AddParameter(NAME("LeftFootOffset"), AnimationGraphParameterType::Float);
    graph->AddParameter(NAME("RightFootOffset"), AnimationGraphParameterType::Float);

    Handle<AnimationMotionMatchingNode> motionNode = MakeHandle<AnimationMotionMatchingNode>();
    motionNode->name = NAME("Motion");
    motionNode->database = database;
    motionNode->editorPosition = Vec2f(0.0f, 0.0f);

    const uint32 motion = graph->AddNode(motionNode);

    Handle<AnimationFootIKNode> footIKNode = MakeHandle<AnimationFootIKNode>();
    footIKNode->name = NAME("FootIK");
    footIKNode->input = motion;
    footIKNode->editorPosition = Vec2f(300.0f, 0.0f);

    graph->SetRootNode(graph->AddNode(footIKNode));

    outputRegistry->PutAssetsDeep(database, /* overwriteExisting */ true);
    outputRegistry->PutAssetsDeep(graph, /* overwriteExisting */ true);

    HYP_LOG(Engine, Info, "HumanoidMotion database built ({} frames) with the HumanoidMotionMatching graph.", database->NumFrames());
}

// the AnimToolkit plugin's content, where its graphs live; the engine's clips they play stay in the engine content
FilePath GetAnimationPluginContentDirectory()
{
    return CoreApi::GetBaseDirectory() / "Source" / "Plugins" / "AnimToolkit" / "Content";
}

} // namespace

class BuildAnimationContentCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildAnimationContentCommandlet);

public:
    virtual ~BuildAnimationContentCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "parts",
                "p",
                "Comma separated parts to build: locomotiongraph, motionmatching. Everything when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));

            s_definitions.Add(
                "output",
                "o",
                "Directory the content is written to. The AnimToolkit plugin's Content when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String parts = args["parts"].ToString();
        const String outputPath = args["output"].ToString();

        if (IsOnThread(g_simThread))
        {
            RunStatic(parts, outputPath);
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [parts, outputPath]()
                {
                    RunStatic(parts, outputPath);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return {};
    }

    static void RunStatic(const String& parts, const String& outputPath)
    {
        const Array<String> selectedParts = parts.Split(',');

        const auto isSelected = [&parts, &selectedParts](const char* part)
        {
            return parts.Empty() || selectedParts.Contains(String(part));
        };

        Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

        if (!engineRegistry.IsValid())
        {
            engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);

            SetEngineAssetRegistry(engineRegistry);
        }

        const FilePath outputDirectory = outputPath.Empty() ? GetAnimationPluginContentDirectory() : FilePath(outputPath);

        if (!outputDirectory.Exists() && !outputDirectory.MkDir())
        {
            HYP_LOG(Engine, Error, "Animation content: could not create {}", outputDirectory);
            return;
        }

        // keeps the Engine id, so the content resolves once its directory is mounted on the engine registry
        Handle<AssetRegistry> outputRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Engine, outputDirectory);
        outputRegistry->Initialize(nullptr);

        if (isSelected("locomotiongraph"))
        {
            BuildHumanoidLocomotionGraph(engineRegistry, outputRegistry);
        }

        if (isSelected("motionmatching"))
        {
            BuildHumanoidMotionMatching(engineRegistry, outputRegistry);
        }

        GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };
        GetCurrentAssetRegistry()->SaveDirtyAssets();

        HYP_LOG(Engine, Info, "Animation content saved to {}", outputRegistry->GetRootPath());
    }
};

HYP_EXPORT const Class* g_clsBuildAnimationContentCommandlet = nullptr;

const Class* BuildAnimationContentCommandlet::StaticClass()
{
    return g_clsBuildAnimationContentCommandlet;
}

HYP_BEGIN_CLASS(BuildAnimationContentCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "buildanimationcontent"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(BuildAnimationContentCommandlet);

#endif // HYP_EDITOR

} // namespace Hyperion
