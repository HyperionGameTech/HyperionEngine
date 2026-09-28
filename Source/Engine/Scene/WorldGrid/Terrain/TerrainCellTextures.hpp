/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Name/Name.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

class Texture;
class TerrainCellData;
class TerrainGenerator;
struct StreamingCellInfo;

/// per-cell textures the terrain material samples. Upload bytes are row-flipped so every map shares Terrain.hlsl's texcoords
namespace TerrainCellTextures {

Handle<Texture> CreateCellTexture(Name name, uint32 cellSize, const Array<ubyte>& uploadBytes);
Handle<Texture> CreateSplatTexture(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& uploadBytes);

void PrepareNormalMapBytes(Span<const float> paddedHeights, Span<const ubyte> erosionMasks, uint32 cellSize, const Vec3f& scale, Array<ubyte>& outUploadBytes);

bool PreparePaintedSplatBytes(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize, Array<ubyte>& outUploadBytes);

bool PrepareAutoSplatBytes(
    const TerrainGenerator& generator,
    const StreamingCellInfo& cellInfo,
    Span<const float> paddedHeights,
    Span<const ubyte> erosionMasks,
    Array<ubyte>& outUploadBytes);

Handle<Texture> BuildPaintedSplatTexture(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize);

} // namespace TerrainCellTextures

} // namespace Hyperion
