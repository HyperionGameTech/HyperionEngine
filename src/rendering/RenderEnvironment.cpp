/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/GaussianSplatting.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

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
