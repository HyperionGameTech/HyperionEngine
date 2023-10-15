#ifndef HYPERION_V2_OGRE_XML_MODEL_LOADER_H
#define HYPERION_V2_OGRE_XML_MODEL_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>
#include <rendering/Mesh.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class OgreXMLModelLoader : public AssetLoader
{
public:
    struct OgreXMLModel
    {
        struct SubMesh
        {
            Array<Mesh::Index> indices;
        };

        struct BoneAssignment
        {
            UInt    index;
            Float   weight;
        };

        String                                  filepath;

        Array<Vector3>                          positions;
        Array<Vector3>                          normals;
        Array<Vector2>                          texcoords;

        Array<Vertex>                           vertices;

        Array<SubMesh>                          submeshes;
        FlatMap<UInt, Array<BoneAssignment>>    bone_assignments;

        String                                  skeleton_name;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif