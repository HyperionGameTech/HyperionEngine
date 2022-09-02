#ifndef HYPERION_V2_HYPERION_HPP
#define HYPERION_V2_HYPERION_HPP

#include <system/SDLSystem.hpp>
#include <system/Debug.hpp>

#include <Engine.hpp>
#include <animation/Bone.hpp>

#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <rendering/ProbeSystem.hpp>
#include <rendering/vct/VoxelConeTracing.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/post_fx/SSAO.hpp>
#include <rendering/post_fx/FXAA.hpp>
#include <rendering/post_fx/Tonemap.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/render_components/CubemapRenderer.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AABBDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <terrain/controllers/TerrainPagingController.hpp>

#include <camera/FirstPersonCamera.hpp>
#include <camera/FollowCamera.hpp>

#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Proc.hpp>
#include <core/system/SharedMemory.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/img/Bitmap.hpp>

#include <GameThread.hpp>
#include <Game.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>

#include <script/ScriptBindings.hpp>

#include <builders/MeshBuilder.hpp>

#include <util/Profile.hpp>
#include <util/UTF8.hpp>

#endif