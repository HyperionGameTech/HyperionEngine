/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectBase.hpp>
#include <Core/containers/SparsePagedArray.hpp>

#include <Core/Types.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

namespace Hyperion {

class ShaderManager;
class FullScreenPass;
class Mesh;
class SwapchainBase;
struct RenderSetup;

class FinalPass final
{
public:
    FinalPass();
    FinalPass(const FinalPass& other) = delete;
    FinalPass& operator=(const FinalPass& other) = delete;
    ~FinalPass();

    void SetUILayerImageView(const GpuImageViewRef& imageView);

    void Create();
    void Render(Frame* frame, const RenderSetup& rs);

private:
    Handle<Mesh> m_quadMesh;
    GpuImageViewRef m_uiLayerImageView;
};
} // namespace Hyperion
