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
            std::vector<Mesh::Index> indices;
        };

        struct BoneAssignment
        {
            UInt index;
            float weight;
        };

        String filepath;

        std::vector<Vector3> positions;
        std::vector<Vector3> normals;
        std::vector<Vector2> texcoords;

        std::vector<Vertex> vertices;

        std::vector<SubMesh> submeshes;
        std::unordered_map<UInt, std::vector<BoneAssignment>> bone_assignments;

        std::string skeleton_name;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif