/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/GroundCover.hpp>

#include <Scene/Prefab.hpp>

#include <GroundCover.generated.inl>

namespace Hyperion {

GroundCover::GroundCover()
    : GroundCover(Name::Invalid())
{
}

GroundCover::GroundCover(Name name)
    : AssetObject(name)
{
}

Name GroundCover::FindPaintedLayer(const Handle<Prefab>& prefab) const
{
    if (!prefab.IsValid())
    {
        return Name::Invalid();
    }

    for (const GroundCoverLayer& layer : layers)
    {
        if (layer.source == GroundCoverSource::Painted && layer.name.IsValid() && layer.types.Size() == 1 && layer.types[0].prefab == prefab)
        {
            return layer.name;
        }
    }

    return Name::Invalid();
}

Name GroundCover::EnsurePaintedLayer(const Handle<Prefab>& prefab)
{
    if (!prefab.IsValid())
    {
        return Name::Invalid();
    }

    if (Name existingLayer = FindPaintedLayer(prefab); existingLayer.IsValid())
    {
        return existingLayer;
    }

    const auto isNameTaken = [this](Name candidate)
    {
        return layers.FindIf([candidate](const GroundCoverLayer& layer)
                   {
                       return layer.name == candidate;
                   })
            != layers.End();
    };

    // cells store what was painted under the layer's name, so it must not collide with another layer's
    Name layerName = prefab->GetName();

    for (uint32 suffix = 2; !layerName.IsValid() || isNameTaken(layerName); suffix++)
    {
        layerName = NAME_FMT("{}_{}", prefab->GetName(), suffix);
    }

    GroundCoverLayer& layer = layers.EmplaceBack();
    layer.name = layerName;
    layer.source = GroundCoverSource::Painted;
    layer.types.PushBack(GroundCoverType { prefab });

    return layerName;
}

Handle<Prefab> GroundCover::GetPaintedLayerPrefab(Name layerName) const
{
    for (const GroundCoverLayer& layer : layers)
    {
        if (layer.source == GroundCoverSource::Painted && layer.name == layerName && layer.types.Size() == 1)
        {
            return layer.types[0].prefab;
        }
    }

    return Handle<Prefab>::empty;
}

HashCode GroundCover::GetContentHashCode() const
{
    HashCode hashCode;

    for (const GroundCoverLayer& layer : layers)
    {
        hashCode.Add(layer.name);
        hashCode.Add(layer.source);
        hashCode.Add(layer.splatLayer);
        hashCode.Add(layer.density);
        hashCode.Add(layer.clumpSize);
        hashCode.Add(layer.types.Size());

        for (const GroundCoverType& type : layer.types)
        {
            hashCode.Add(static_cast<const void*>(type.prefab.Get()));
            hashCode.Add(type.weight);
            hashCode.Add(type.colorVariation);
            hashCode.Add(type.groundNormalBlend);
            hashCode.Add(type.baseOcclusion);
        }
    }

    return hashCode;
}

} // namespace Hyperion
