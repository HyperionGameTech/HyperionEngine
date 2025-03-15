/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OBJ_MODEL_LOADER_HPP
#define HYPERION_OBJ_MODEL_LOADER_HPP

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Tuple.hpp>

#include <Types.hpp>

namespace hyperion {

class OBJModelLoader : public AssetLoader
{
public:
    struct OBJModel
    {
        struct OBJIndex
        {
            int64 vertex,
                  normal,
                  texcoord;

            HYP_FORCE_INLINE bool operator==(const OBJIndex &other) const
                { return vertex == other.vertex
                    && normal == other.normal
                    && texcoord == other.texcoord; }

            HYP_FORCE_INLINE bool operator<(const OBJIndex &other) const
                { return Tie(vertex, normal, texcoord) < Tie(other.vertex, other.normal, other.texcoord); }

            HYP_FORCE_INLINE HashCode GetHashCode() const
            {
                HashCode hc;
                hc.Add(vertex);
                hc.Add(normal);
                hc.Add(texcoord);

                return hc;
            }
        };

        struct OBJMesh
        {
            String          name;
            String          material;
            Array<OBJIndex> indices;
        };

        String          filepath;

        Array<Vec3f>    positions;
        Array<Vec3f>    normals;
        Array<Vec2f>    texcoords;
        Array<OBJMesh>  meshes;
        String          name;
        String          material_library;
    };

    virtual ~OBJModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState &state) const override
    {
        OBJModel model = LoadModel(state);

        return BuildModel(state, model);
    }
    
    static OBJModel LoadModel(LoaderState &state);
    static LoadedAsset BuildModel(LoaderState &state, OBJModel &model);
};

using OBJIndex = OBJModelLoader::OBJModel::OBJIndex;

} // namespace hyperion

namespace std {
template<>
struct hash<hyperion::OBJIndex> {
    size_t operator()(const hyperion::OBJIndex &obj) const
    {
        hyperion::HashCode hc;
        hc.Add(obj.vertex);
        hc.Add(obj.normal);
        hc.Add(obj.texcoord);

        return hc.Value();
    }
};
} // namespace std

#endif