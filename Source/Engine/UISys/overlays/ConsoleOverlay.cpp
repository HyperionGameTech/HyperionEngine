/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <ui/UIStage.hpp>

#include <ui/overlays/ConsoleOverlay.hpp>

#include <ui/console/UIConsole.hpp>

#include <ConsoleOverlay.generated.inl>

namespace Hyperion {

#pragma region ConsoleOverlay

ConsoleOverlay::ConsoleOverlay()
    : OverlayBase()
{
}

ConsoleOverlay::~ConsoleOverlay() = default;

Handle<UIObject> ConsoleOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIConsole> console = spawnParent->CreateUIObject<UIConsole>(
        NAME("ConsoleOverlay_Console"),
        Vec2i(2, 2),
        UIObjectSize({ 500, UIObjectSize::PIXEL }, { 200, UIObjectSize::PIXEL }));

    console->SetDepth(1000);

    m_console = console.Get();

    return console;
}

void ConsoleOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

}

#pragma endregion ConsoleOverlay

} // namespace Hyperion
