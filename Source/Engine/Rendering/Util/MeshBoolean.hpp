/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Rendering/Mesh.hpp>

namespace Hyperion {

enum class MeshBooleanOperation : uint8
{
    Union = 0,
    Subtract,
    Intersect
};

enum class MeshBooleanError : uint8
{
    None = 0,
    NotSupported,
    UnsupportedLayout,
    MissingData,
    TargetNotClosed,
    BrushNotClosed,
    EmptyResult,
    Failed
};

struct MeshBooleanOperand
{
    const Mesh* mesh = nullptr;

    Mat4f transform = Mat4f::Identity();
};

struct MeshBooleanResult
{
    MeshBooleanError error = MeshBooleanError::None;

    MeshDesc meshDesc;
    Array<float> vertexData;
    Array<uint32> indices;
};

class ENGINE_API MeshBoolean final
{
public:
    static bool IsSupported();

    static bool IsLayoutSupported(const VertexInputLayoutDesc& inputLayout);

    static MeshBooleanError ValidateSolid(const Mesh* mesh);

    static MeshBooleanResult Apply(const MeshBooleanOperand& target, const MeshBooleanOperand& brush, MeshBooleanOperation operation);

    static const char* GetErrorMessage(MeshBooleanError error);
};

} // namespace Hyperion
