#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>
#include <engine/EngineDriver.hpp>

using namespace hyperion;

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }
    
    Handle<HyperionEditor> editorInstance = CreateObject<HyperionEditor>();
    
    App::GetInstance().LaunchGame(editorInstance);

    Hyp_Shutdown();
    
    return 0;
}
