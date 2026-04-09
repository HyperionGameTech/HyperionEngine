/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/RenderObject.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/containers/Array.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

class FullScreenPass;
class Texture;

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer
{
public:
    static void RenderMipmaps(const Handle<Texture>& texture);
};

#pragma endregion TextureMipmapRenderer

} // namespace Hyperion