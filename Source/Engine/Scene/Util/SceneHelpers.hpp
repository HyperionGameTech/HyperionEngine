/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once


namespace Hyperion {

class Camera;
class World;

namespace SceneHelpers {

Camera* FindMainCamera(World& world);

} // namespace SceneHelpers

} // namespace Hyperion
