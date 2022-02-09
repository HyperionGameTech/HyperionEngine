#ifndef NOISE_TERRAIN_CONTROL_H
#define NOISE_TERRAIN_CONTROL_H

#include "../terrain_control.h"
#include "noise_terrain_chunk.h"

namespace hyperion {

class NoiseTerrainControl : public TerrainControl {
public:
    NoiseTerrainControl(Camera *camera, int seed=123);
    virtual ~NoiseTerrainControl() = default;

#pragma region serialization
    FBOM_DEF_DESERIALIZER(loader, in, out) {
        using namespace fbom;

        int32_t seed;
        in->GetProperty("seed").ReadInt(&seed);

        out = std::make_shared<NoiseTerrainControl>(nullptr, seed);

        return FBOMResult::FBOM_OK;
    }

    FBOM_DEF_SERIALIZER(loader, in, out)
    {
        using namespace fbom;

        auto object = dynamic_cast<NoiseTerrainControl*>(in);

        if (object == nullptr) {
            return FBOMResult::FBOM_ERR;
        }

        out->SetProperty("seed", FBOMInt(), sizeof(int32_t), (void*)&object->seed);

        return FBOMResult::FBOM_OK;
    }
#pragma endregion serialization

protected:
    int seed;

    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    virtual std::shared_ptr<TerrainChunk> NewChunk(const ChunkInfo &height_info) override;
};

} // namespace hyperion

#endif
