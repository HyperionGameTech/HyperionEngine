/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifdef __cplusplus

#include <HyperionPch.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineMemory.hpp>

// every backend's PCH is folded into the engine's shared one, whichever backend is being built
#if defined(HYP_WEBGPU) && HYP_WEBGPU
#include <Rendering/WebGPU/WebGPUShared.hpp>
#endif

#endif // __cplusplus
