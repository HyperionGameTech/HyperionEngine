/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/math/Vertex.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/Types.hpp>

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

        Array<Vertex> vertices;

        Array<SubMesh> submeshes;
        FlatMap<uint32, Array<BoneAssignment>> boneAssignments;

        String skeletonName;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
