#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/World.hpp>

#include <Rendering/Texture.hpp>

#include <UI/UISubsystem.hpp>

#include <UI/Overlays/Overlay.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

class ShowTexture final : public CommandletBase
{
    HYP_OBJECT_BODY(ShowTexture);

public:
    virtual ~ShowTexture() override = default;

    struct WatchTextureState
    {
        Handle<TextureOverlay> overlay;
        AssetPath path;

        explicit operator bool() const
        {
            return path.IsValid();
        }
    };

    static WatchTextureState s_watchTextureState;
    static Mutex s_watchTextureStateMtx;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "texture",
                "t",
                "Texture name to display",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        // temp debug
        const String textureName = args["texture"].ToString();

        // remove existing overlay and stop watching if active
        {
            Mutex::Guard guard(s_watchTextureStateMtx);
            if (s_watchTextureState)
            {
                if (s_watchTextureState.overlay.IsValid())
                {
                    GetThreadById(g_simThread)->GetScheduler().Enqueue(
                        [overlay = s_watchTextureState.overlay]()
                        {
                            if (UISubsystem* uiSubsystem = TryGetUISubsystem())
                            {
                                if (overlay.IsValid())
                                {
                                    uiSubsystem->RemoveDebugOverlay(overlay);
                                }
                            }
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                }

                s_watchTextureState = {};
            }
        }

        // just hide the overlay
        if (textureName.Empty())
        {
            return {};
        }

        const auto tryLoadFromRegistry = [&textureName](AssetRegistry& registry) -> Handle<Texture>
        {
            return DynamicCast<Texture>(registry.GetAsset(AssetBuckets::Textures, StringHash(textureName)));
        };

        Handle<Texture> texture = tryLoadFromRegistry(*GetCurrentAssetRegistry());
        if (!texture.IsValid())
        {
            texture = tryLoadFromRegistry(*GetEngineAssetRegistry());
        }
#if HYP_EDITOR
        if (!texture.IsValid())
        {
            texture = tryLoadFromRegistry(*GetEditorAssetRegistry());
        }
#endif // HYP_EDITOR

        if (!texture.IsValid())
        {
            HYP_LOG(Console, Error, "ShowTexture: texture '{}' not found", textureName);
            return HYP_MAKE_ERROR(Error, "Texture '{}' not found", textureName);
        }

        Handle<TextureOverlay> overlay = MakeHandle<TextureOverlay>(texture);
        InitObject(overlay);

        Mutex::Guard guard(s_watchTextureStateMtx);
        s_watchTextureState.overlay = overlay;
        s_watchTextureState.path = texture->GetPath();

        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [overlay]()
            {
                if (UISubsystem* uiSubsystem = TryGetUISubsystem())
                {
                    uiSubsystem->AddDebugOverlay(overlay);
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return {};
    }

private:
    static UISubsystem* TryGetUISubsystem()
    {
        World* world = EngineDriver::GetInstance()->GetCurrentWorld();

        if (world != nullptr)
        {
            return world->GetSubsystem<UISubsystem>();
        }

        return nullptr;
    }
};

// static definition
ShowTexture::WatchTextureState ShowTexture::s_watchTextureState;
Mutex ShowTexture::s_watchTextureStateMtx;

ENGINE_API const Class* g_clsShowTexture = nullptr;

const Class* ShowTexture::StaticClass()
{
    return g_clsShowTexture;
}

// clang-format off

HYP_BEGIN_CLASS(ShowTexture, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "showtexture"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(ShowTexture);

} // namespace Hyperion
