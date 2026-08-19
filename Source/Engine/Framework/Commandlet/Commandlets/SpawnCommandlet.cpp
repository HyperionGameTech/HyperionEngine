#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

class SpawnCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(SpawnCommandlet);

public:
    virtual ~SpawnCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "pos", "", "Position to spawn at",
                CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value("0,0,0"));

            s_definitions.Add(
                "type", "", "Type of Entity to spawn",
                CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value("Entity"));

            s_definitions.Add(
                "name", "", "Name of spawned entity",
                CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));

            s_definitions.Add(
                "scene", "", "Name of the scene to spawn into",
                CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String posStr = args["pos"].ToString();
        const String typeName = args["type"].ToString();
        const String entityName = args["name"].ToString();
        const String sceneName = args["scene"].ToString();

        Vec3f position = Vec3f::Zero();

        Array<String> posParts = posStr.Split(',');

        if (posParts.Size() == 3)
        {
            position = Vec3f(
                StringUtil::Parse<float>(posParts[0].Trimmed()),
                StringUtil::Parse<float>(posParts[1].Trimmed()),
                StringUtil::Parse<float>(posParts[2].Trimmed()));
        }
        else if (posStr.Any())
        {
            return HYP_MAKE_ERROR(Error, "Invalid position to spawn");
        }

        const Class* entityClass = ClassRegistry::GetInstance().GetClass(ANSIString(typeName), /* ignoreCase */ true);

        if (entityClass == nullptr
            || !entityClass->IsDerivedFrom(Entity::StaticClass())
            || entityClass->IsAbstract())
        {
            return HYP_MAKE_ERROR(Error, "'{}' is not a valid Entity subclass", typeName);
        }

        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [position, entityClass, entityName, sceneName]()
            {
                World* world = EngineDriver::GetInstance()->GetCurrentWorld();

                if (!world)
                {
                    HYP_LOG(Console, Error, "No active world to spawn into");

                    return;
                }

                Scene* targetScene = nullptr;

                for (const Handle<Scene>& scene : world->GetScenes())
                {
                    if (sceneName.Any())
                    {
                        if (scene->GetName() == CreateNameFromDynamicString(ANSIString(sceneName)))
                        {
                            targetScene = scene;

                            break;
                        }
                    }
                    else if (scene->IsForegroundScene())
                    {
                        targetScene = scene;

                        break;
                    }
                }

                if (targetScene == nullptr)
                {
                    HYP_LOG(Console, Error, "Could not find a scene to spawn into (requested: '{}')",
                        sceneName.Any() ? sceneName : "<first foreground scene>");

                    return;
                }

                Handle<Entity> entity = targetScene->GetEntityManager()->AddTypedEntity(entityClass);

                if (!entity.IsValid())
                {
                    HYP_LOG(Console, Error, "Spawn: failed to create instance of '{}'", entityClass->GetName());

                    return;
                }

                if (entityName.Any())
                {
                    entity->SetName(CreateNameFromDynamicString(ANSIString(entityName)));
                }

                targetScene->GetRoot()->AddChild(entity);

                entity->SetLocalTransform(Transform(position));
                entity->AddTag<EntityTag::Replicated>();

                HYP_LOG(Console, Debug, "Spawn: spawned {} '{}' at ({}, {}, {}) into scene '{}'",
                    entityClass->GetName(), entity->GetName(), position.x, position.y, position.z, targetScene->GetName());
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return {};
    }
};

ENGINE_API const Class* g_clsSpawnCommandlet = nullptr;

const Class* SpawnCommandlet::StaticClass()
{
    return g_clsSpawnCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(SpawnCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "spawn"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(SpawnCommandlet);

} // namespace Hyperion
