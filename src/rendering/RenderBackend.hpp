/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

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