/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetLoader.hpp>

#include <Rendering/Vertex.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class OgreXMLModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(OgreXMLModelLoader);

public:
    struct OgreXMLModel
    {
        struct SubMesh
        {
            String name;
            Array<uint32> indices;
        };

        struct BoneAssignment
        {
            uint32 index;
            float weight;
        };

        String filepath;

        Array<Vec3f> positions;
        Array<Vec3f> normals;
        Array<Vec2f> texcoords;

        Array<float> vertexData;

        Array<SubMesh> submeshes;
        FlatMap<uint32, Array<BoneAssignment>> boneAssignments;

        String skeletonName;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
