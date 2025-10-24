/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/reflection/Handle.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class FullScreenPass;
class Texture;

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer
{
public:
    static void RenderMipmaps(const Handle<Texture>& texture);
};

#pragma endregion TextureMipmapRenderer

} // namespace hyperion