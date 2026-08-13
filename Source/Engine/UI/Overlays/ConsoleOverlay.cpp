/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/UIStage.hpp>

#include <UI/Overlays/ConsoleOverlay.hpp>

#include <UI/Console/UIConsole.hpp>

#include <ConsoleOverlay.generated.inl>

namespace Hyperion {

#pragma region ConsoleOverlay

ConsoleOverlay::ConsoleOverlay()
    : OverlayBase()
{
}

ConsoleOverlay::~ConsoleOverlay() = default;

Handle<UIObject> ConsoleOverlay::CreateUIObject(UIObject* spawnParent)
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

void ConsoleOverlay::Update(float delta)
{
    HYP_SCOPE;

}

#pragma endregion ConsoleOverlay

} // namespace Hyperion
