#include <HyperionPch.hpp>

#include <engine/console/ConsoleCommand.hpp>

#include <core/reflection/ClassUtils.hpp>

#include <core/threading/Task.hpp>

#include <scene/View.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

struct DumpRenderCollectorsImpl final : RenderCommand
{
    DumpRenderCollectorsImpl()
    {
    }

    virtual RendererResult operator()() override
    {
        Array<Pair<View*, RenderCollector*>> result = RenderApi::GetAllRenderCollectors();

        for (const auto& pair : result)
        {
            View* view = pair.first;
            RenderCollector* collector = pair.second;

            HYP_LOG(Console, Info, "View: {}, RenderCollector: {}", view->Id(), (void*)collector);

            RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
            rpl.BeginRead();

            HYP_LOG(Console, Info, "RenderProxyList: {}", (void*)&rpl);

            HYP_LOG(Console, Info, "Textures: {}", rpl.GetTextures().NumCurrent());
            HYP_LOG(Console, Info, "Meshes: {}", rpl.GetMeshEntities().NumCurrent());
            HYP_LOG(Console, Info, "Materials: {}", rpl.GetMaterials().NumCurrent());
            HYP_LOG(Console, Info, "Skeletons: {}", rpl.GetSkeletons().NumCurrent());
            HYP_LOG(Console, Info, "Lights: {}", rpl.GetLights().NumCurrent());
            HYP_LOG(Console, Info, "EnvProbes: {}", rpl.GetEnvProbes().NumCurrent());
            HYP_LOG(Console, Info, "EnvGrids: {}", rpl.GetEnvGrids().NumCurrent());

            rpl.EndRead();
        }

        return {};
    }
};

class DumpRenderCollectors : public ConsoleCommandBase
{
    HYP_OBJECT_BODY(DumpRenderCollectors);

public:
    virtual ~DumpRenderCollectors() override = default;

protected:
    virtual Result Execute_Impl(const CommandLineArguments& args) override
    {
        PUSH_RENDER_COMMAND(DumpRenderCollectorsImpl);

        return {}; // ok
    }
};

HYP_API const Class* g_clsDumpRenderCollectors = nullptr;

HYP_BEGIN_CLASS(DumpRenderCollectors, -1, 0, NAME("ConsoleCommandBase"), ClassAttribute("command", "dumprendercollectors"))
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(DumpRenderCollectors);

} // namespace hyperion
