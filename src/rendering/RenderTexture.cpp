/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>

#include <scene/Texture.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region TextureRenderResource

TextureRenderResource::TextureRenderResource(Texture *texture)
    : m_texture(texture)
{
}

TextureRenderResource::~TextureRenderResource() = default;

void TextureRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

}

void TextureRenderResource::Destroy_Internal()
{
    HYP_SCOPE;
}

void TextureRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *TextureRenderResource::GetGPUBufferHolder() const
{
    return nullptr;
}

#pragma endregion TextureRenderResource

} // namespace hyperion
