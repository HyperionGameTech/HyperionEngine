#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "../asset_loader.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"

#include <vector>
#include <string>
#include <map>

namespace hyperion {
class MtlLib;

struct ObjModel {
    struct ObjIndex {
        int vertex_idx, normal_idx, texcoord_idx;
    };

    std::vector<std::string> mesh_names;
    std::map<std::string, std::string> mesh_material_names;
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<std::vector<ObjIndex>> indices;
    std::shared_ptr<MtlLib> mtl_lib;

    bool has_normals, has_texcoords;

    ObjModel()
        : has_normals(false),
          has_texcoords(false)
    {
    }

    void AddMesh(const std::string &name);
    std::vector<ObjIndex> &CurrentList();
    ObjIndex ParseObjIndex(const std::string &);
};

class ObjLoader : public AssetLoader {
public:
    virtual ~ObjLoader() = default;

    virtual Result LoadFromFile(const std::string &) override;
};
}

#endif
