/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>

#include <rendering/raytracing/RenderAccelerationStructure.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/DebugDrawer.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

RenderEnvironment::RenderEnvironment()
{
}

RenderEnvironment::~RenderEnvironment()
{
    m_particleSystem.Reset();
    m_gaussianSplatting.Reset();
}

void RenderEnvironment::Initialize()
{
    m_particleSystem = CreateObject<ParticleSystem>();
    InitObject(m_particleSystem);

    m_gaussianSplatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussianSplatting);
}

} // namespace hyperion
