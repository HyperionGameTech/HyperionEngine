/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/Span.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/RefCountedPtr.hpp>

namespace Hyperion {

class RenderableAttributeSet;
class Shader;
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
