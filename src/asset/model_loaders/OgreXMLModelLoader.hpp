/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OGRE_XML_MODEL_LOADER_HPP
#define HYPERION_OGRE_XML_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>

#include <Types.hpp>

namespace hyperion {

class OgreXMLModelLoader : public AssetLoader
{
public:
    struct OgreXMLModel
    {
        struct SubMesh
        {
            String          name;
            Array<uint32>   indices;
        };

        struct BoneAssignment
        {
            uint32  index;
            float   weight;
        };

        String                                  filepath;

        Array<Vec3f>                            positions;
        Array<Vec3f>                            normals;
        Array<Vec2f>                            texcoords;

        Array<Vertex>                           vertices;

        Array<SubMesh>                          submeshes;
        FlatMap<uint32, Array<BoneAssignment>>  bone_assignments;

        String                                  skeleton_name;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif