#include <ui/controllers/UIController.hpp>

namespace hyperion::v2 {

UIController::UIController(const String &name, bool receives_update)
    : Controller(name, receives_update)
{
}

} // namespace hyperion::v2