#ifndef POPULATOR_H
#define POPULATOR_H

#include "../../entity.h"
#include "../../control.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"
#include "../../util/random/simplex.h"

#include <vector>
#include <memory>

namespace hyperion {
class TerrainChunk;
class Camera;
class NoiseGenerator;
class Populator : public EntityControl {
public:
    struct Patch;

    Populator(
        const fbom::FBOMType &loadable_type,
        Camera *camera,
        unsigned long seed = 12345,
        double probability_factor = 0.55,
        float tolerance = 0.15f,
        float max_distance = 150.0f,
        float spread = 1.5f,
        int num_entities_per_chunk = 4,
        int num_patches = 2,
        int patch_spread = -1,
        bool use_batching = true
    );
    virtual ~Populator();

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnFirstRun(double dt) override;
    virtual void OnUpdate(double dt) override;

    virtual std::shared_ptr<Entity> CreateEntity(const Vector3 &position) const = 0;

    virtual std::shared_ptr<Entity> CreateEntityNode(Patch &patch);
    virtual double GetNoise(const Vector2 &location) const;
    virtual float GetHeight(const Vector3 &location) const;
    virtual Vector3 GetNormal(const Vector3 &location) const;

    void CreatePatches(
        const Vector2 &origin,
        const Vector2 &center,
        float parent_size
    );

    struct GridTile {
        float width;
        float length;
        float x;
        float z;
        Vector2 center;
        float max_distance;

        GridTile()
            : x(0.0),
              z(0.0),
              width(0.0),
              length(0.0),
              max_distance(0.0),
              center(Vector2::Zero())
        {
        }

        GridTile(float x, float z, float width, float length, float max_distance)
            : x(x),
              z(z),
              width(width),
              length(length),
              max_distance(max_distance),
              center(x + width / 2.0, z + width / 2.0)
        {
        }

        GridTile(const GridTile &other)
            : x(other.x),
              z(other.z),
              width(other.width),
              length(other.length),
              max_distance(other.max_distance),
              center(other.center)
        {
        }

        inline bool Collides(const Vector2 &point, const Vector2 &target, float size)
        {
            if (MathUtil::Round(point.x) >= target.x - size && MathUtil::Round(point.x) <= target.x + size) {
                if (MathUtil::Round(point.y) >= target.y - size && MathUtil::Round(point.y) <= target.y + size) {
                    return true;
                }
            }

            return false;
        }

        inline bool InRange(const Vector3 &point)
        {
            float dist = center.Distance(Vector2(point.x, point.z));

            return dist < max_distance;
        }
    };

    struct Patch {
        std::shared_ptr<Entity> m_node;
        Vector3 m_chunk_start;
        bool m_is_created;
        float m_chunk_size;
        int m_num_entities_per_chunk;
        GridTile m_tile;
        Vector4 m_test_patch_color;

        inline Vector3 GetCenter() const
        {
            return m_chunk_start + Vector3(m_chunk_size / 2.0f, 0, m_chunk_size / 2.0f);
        }

        enum PageState {
            PATCH_WAITING,
            PATCH_UNLOADED,
            PATCH_UNLOADING,
            PATCH_LOADED
        } m_page_state;
    };

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() = 0;

    Camera *m_camera;

    std::shared_ptr<Entity> m_entity;

    std::vector<Patch> m_patches;
    unsigned long m_seed;
    double m_probability_factor;
    float m_tolerance;
    float m_max_distance;
    float m_spread;
    int m_num_entities_per_chunk;
    int m_num_patches;
    int m_patch_spread;
    bool m_use_batching;

private:
    NoiseGenerator *m_noise;
};
} // namespace hyperion

#endif
