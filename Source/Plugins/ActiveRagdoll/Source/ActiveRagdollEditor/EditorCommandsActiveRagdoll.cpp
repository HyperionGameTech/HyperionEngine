/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <ActiveRagdoll/ActiveRagdollSystem.hpp>

#include <Framework/Game.hpp>

#include <Core/Utilities/StringUtil.hpp>

namespace Hyperion {

namespace {

// the simulating world while playing, else the one being edited
World* GetRagdollWorld(EditorSubsystem* subsystem)
{
    const Handle<EditorProject>& project = subsystem->GetCurrentProject();

    if (project.IsValid() && project->GetGame().IsValid() && project->GetGame()->GetWorld().IsValid())
    {
        return project->GetGame()->GetWorld().Get();
    }

    const Handle<Scene> activeScene = subsystem->GetActiveScene();

    if (activeScene.IsValid())
    {
        return activeScene->GetWorld();
    }

    return nullptr;
}

Entity* ResolveRagdollEntity(EditorSubsystem* subsystem, const String& uuidArgument, ActiveRagdollSystem*& outSystem)
{
    outSystem = nullptr;

    World* world = GetRagdollWorld(subsystem);
    const UUID uuid = UUID(uuidArgument.Data());

    if (world == nullptr || uuid == UUID::Invalid())
    {
        HYP_LOG(Editor, Warning, "Active ragdoll: no world, or '{}' isn't a UUID", uuidArgument);

        return nullptr;
    }

    outSystem = world->GetSystem<ActiveRagdollSystem>();

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (Entity* entity = TryCast<Entity>(scene->FindNodeByUUID(uuid)))
        {
            return entity;
        }
    }

    HYP_LOG(Editor, Warning, "Active ragdoll: no entity '{}' in world '{}'", uuidArgument, world->GetName());

    return nullptr;
}

float ParseFloatArgument(const String& argument, float defaultValue)
{
    float value = defaultValue;
    StringUtil::Parse(argument, &value);

    return value;
}

} // namespace

#pragma region RagdollHit

// RagdollHit <uuid> <bone or -> <impulse x> <impulse y> <impulse z> [damage]: hits the bone's body at its centre
class EditorCommandRagdollHit final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollHit);

public:
    virtual ~EditorCommandRagdollHit() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Hit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 5)
        {
            HYP_LOG(Editor, Warning, "RagdollHit needs an entity UUID, a bone (or -) and an impulse");

            return;
        }

        ActiveRagdollSystem* system = nullptr;
        Entity* entity = ResolveRagdollEntity(subsystem, GetArgument(0), system);

        if (entity == nullptr || system == nullptr)
        {
            return;
        }

        uint32 body = system->GetChestBody(entity);

        if (GetArgument(1) != "-")
        {
            body = system->FindBody(entity, Name(GetArgument(1).Data()));
        }

        Vec3f point;

        if (!system->GetBodyPosition(entity, body, point))
        {
            HYP_LOG(Editor, Warning, "RagdollHit: '{}' has no ragdoll body for '{}'", entity->GetName(), GetArgument(1));

            return;
        }

        const Vec3f impulse(ParseFloatArgument(GetArgument(2), 0.0f), ParseFloatArgument(GetArgument(3), 0.0f), ParseFloatArgument(GetArgument(4), 0.0f));

        float damage = 0.0f;

        if (NumArguments() >= 6)
        {
            damage = ParseFloatArgument(GetArgument(5), 0.0f);
        }

        system->Hit(entity, body, point, impulse, damage);

        HYP_LOG(Editor, Info, "RagdollHit: '{}' body {} impulse {}", entity->GetName(), body, impulse);
    }
};

DEFINE_EDITOR_COMMAND(RagdollHit);

#pragma endregion RagdollHit

#pragma region RagdollShoot

// RagdollShoot <origin x y z> <direction x y z> [impulse] [damage]
class EditorCommandRagdollShoot final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollShoot);

public:
    virtual ~EditorCommandRagdollShoot() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Shoot";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 6)
        {
            HYP_LOG(Editor, Warning, "RagdollShoot needs an origin and a direction");

            return;
        }

        World* world = GetRagdollWorld(subsystem);
        ActiveRagdollSystem* system = nullptr;

        if (world != nullptr)
        {
            system = world->GetSystem<ActiveRagdollSystem>();
        }

        if (system == nullptr)
        {
            HYP_LOG(Editor, Warning, "RagdollShoot: no active ragdoll system in the world");

            return;
        }

        const Vec3f origin(ParseFloatArgument(GetArgument(0), 0.0f), ParseFloatArgument(GetArgument(1), 0.0f), ParseFloatArgument(GetArgument(2), 0.0f));
        const Vec3f direction(ParseFloatArgument(GetArgument(3), 0.0f), ParseFloatArgument(GetArgument(4), 0.0f), ParseFloatArgument(GetArgument(5), 1.0f));

        float impulse = 20.0f;
        float damage = 10.0f;

        if (NumArguments() >= 7)
        {
            impulse = ParseFloatArgument(GetArgument(6), impulse);
        }

        if (NumArguments() >= 8)
        {
            damage = ParseFloatArgument(GetArgument(7), damage);
        }

        ActiveRagdollRayHit hit;

        if (!system->CastRay(origin, direction, 1000.0f, hit))
        {
            HYP_LOG(Editor, Info, "RagdollShoot: missed");

            return;
        }

        system->Hit(hit.entity, hit.body, hit.position, direction.Normalized() * impulse, damage);

        HYP_LOG(Editor, Info, "RagdollShoot: hit '{}' bone '{}' at {}", hit.entity->GetName(), hit.boneName, hit.position);
    }
};

DEFINE_EDITOR_COMMAND(RagdollShoot);

#pragma endregion RagdollShoot

#pragma region RagdollPush

// RagdollPush <uuid> <impulse x> <impulse y> <impulse z>
class EditorCommandRagdollPush final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollPush);

public:
    virtual ~EditorCommandRagdollPush() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Push";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 4)
        {
            HYP_LOG(Editor, Warning, "RagdollPush needs an entity UUID and an impulse");

            return;
        }

        ActiveRagdollSystem* system = nullptr;
        Entity* entity = ResolveRagdollEntity(subsystem, GetArgument(0), system);

        if (entity == nullptr || system == nullptr)
        {
            return;
        }

        system->Push(entity, Vec3f(ParseFloatArgument(GetArgument(1), 0.0f), ParseFloatArgument(GetArgument(2), 0.0f), ParseFloatArgument(GetArgument(3), 0.0f)));
    }
};

DEFINE_EDITOR_COMMAND(RagdollPush);

#pragma endregion RagdollPush

#pragma region RagdollKill

// RagdollKill <uuid> [bone]
class EditorCommandRagdollKill final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollKill);

public:
    virtual ~EditorCommandRagdollKill() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Kill";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1)
        {
            HYP_LOG(Editor, Warning, "RagdollKill needs an entity UUID");

            return;
        }

        ActiveRagdollSystem* system = nullptr;
        Entity* entity = ResolveRagdollEntity(subsystem, GetArgument(0), system);

        if (entity == nullptr || system == nullptr)
        {
            return;
        }

        uint32 body = ~0u;

        if (NumArguments() >= 2)
        {
            body = system->FindBody(entity, Name(GetArgument(1).Data()));
        }

        system->Kill(entity, body);
    }
};

DEFINE_EDITOR_COMMAND(RagdollKill);

#pragma endregion RagdollKill

#pragma region RagdollLimp

// RagdollLimp <uuid>
class EditorCommandRagdollLimp final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollLimp);

public:
    virtual ~EditorCommandRagdollLimp() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Limp";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1)
        {
            HYP_LOG(Editor, Warning, "RagdollLimp needs an entity UUID");

            return;
        }

        ActiveRagdollSystem* system = nullptr;
        Entity* entity = ResolveRagdollEntity(subsystem, GetArgument(0), system);

        if (entity != nullptr && system != nullptr)
        {
            system->GoLimp(entity);
        }
    }
};

DEFINE_EDITOR_COMMAND(RagdollLimp);

#pragma endregion RagdollLimp

#pragma region RagdollBehaviour

// RagdollBehaviour <uuid> <name> [stop]
class EditorCommandRagdollBehaviour final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRagdollBehaviour);

public:
    virtual ~EditorCommandRagdollBehaviour() override = default;

    virtual String GetText() const override
    {
        return "Ragdoll Behaviour";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "RagdollBehaviour needs an entity UUID and a behaviour name");

            return;
        }

        ActiveRagdollSystem* system = nullptr;
        Entity* entity = ResolveRagdollEntity(subsystem, GetArgument(0), system);

        if (entity == nullptr || system == nullptr)
        {
            return;
        }

        const Name behaviourName = Name(GetArgument(1).Data());
        bool result = false;

        if (NumArguments() >= 3 && GetArgument(2) == "stop")
        {
            result = system->StopBehaviour(entity, behaviourName);
        }
        else
        {
            result = system->TriggerBehaviour(entity, behaviourName);
        }

        HYP_LOG(Editor, Info, "RagdollBehaviour: '{}' {} -> {}", entity->GetName(), behaviourName, result);
    }
};

DEFINE_EDITOR_COMMAND(RagdollBehaviour);

#pragma endregion RagdollBehaviour

} // namespace Hyperion
