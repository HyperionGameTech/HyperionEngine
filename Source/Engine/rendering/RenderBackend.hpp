/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/Span.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderMemory.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/RefCountedPtr.hpp>

namespace Hyperion {

class RenderableAttributeSet;
class Shader;
class Material;
class FrameBase;
class SwapchainBase;
class AsyncComputeBase;
struct TextureDesc;
class SingleTimeCommands;
class Texture;
class ApplicationWindow;

class DescriptorSetLayout;
struct ShaderInputGroup;

enum class GpuBufferType : uint8;
enum RenderTargetType : uint8;

template <class T>
struct Handle;

} // namespace Hyperion