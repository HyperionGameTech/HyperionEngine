/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Asset/AssetPath.hpp>

#include <Baking/BakeEpoch.hpp>

namespace Hyperion {

namespace Baking {

HYP_STRUCT()
struct BakerSceneCategory
{
    HYP_STRUCT_BODY(BakerSceneCategory);

    enum { LightProvider, LightReceiver, Lightmap, Max };

    /// The category name
    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "Revision", Serialize)
    uint64 revision = 0;

    /// Per-asset epochs
    HYP_FIELD(Property = "Assets", Serialize)
    Array<Pair<String, uint64>> assets;
};

HYP_STRUCT()
struct BakerScene
{
    HYP_STRUCT_BODY(BakerScene);
    
    HYP_FIELD(Property = "Categories", Serialize)
    Array<BakerSceneCategory> categories;

    BakerScene()
    {
        categories.Resize(BakerSceneCategory::Max);
        categories[BakerSceneCategory::LightProvider] = { NAME("LightProvider") };
        categories[BakerSceneCategory::LightReceiver] = { NAME("LightReceiver") };
        categories[BakerSceneCategory::Lightmap] = { NAME("Lightmap") };
    }

    template <class T>
    static String GetNodePath(const T& obj)
    {
        return obj.GetScene()->GetName().ToString()
            + '/'
            + String::Join(MapToArray(obj.GetDeepPath(), &Name::ToString), '/');
    }

    template <int Category, class T>
    bool Add(const T& obj)
    {
        if (!obj.GetScene()) return false; // not attached to a scene (???)

        const String nodePath = GetNodePath(obj);

        auto it = categories[Category].assets.FindIf([&](const Pair<String, uint64> &pair)
        {
            return pair.first == nodePath;
        });

        if (it == categories[Category].assets.End())
        {
            categories[Category].assets.EmplaceBack(nodePath, 0);

            return true;
        }

        return false;
    }

    template <int Category, class T>
    bool Remove(const T& obj)
    {
        if (!obj.GetScene()) return false; // not attached to a scene (???)

        const String nodePath = GetNodePath(obj);

        auto it = categories[Category].assets.FindIf([&](const Pair<String, uint64> &pair)
        {
            return pair.first == nodePath;
        });

        if (it != categories[Category].assets.End())
        {
            categories[Category].assets.Erase(it);

            return true;
        }

        return false;
    }

    template <int Category, class T>
    void SetAssetEpoch(const T& obj, uint64 epoch)
    {
        if (!obj.GetScene()) return;

        const String nodePath = GetNodePath(obj);

        auto it = categories[Category].assets.FindIf([&](const Pair<String, uint64>& pair)
        {
            return pair.first == nodePath;
        });

        if (it != categories[Category].assets.End())
        {
            it->second = epoch;

            return;
        }

        // not tracked, add it
        categories[Category].assets.EmplaceBack(nodePath, epoch);
    }

    template <int Category, class T>
    bool TryGetAssetEpoch(const T& obj, uint64& outEpoch) const
    {
        if (!obj.GetScene()) return false;

        const String nodePath = GetNodePath(obj);

        auto it = categories[Category].assets.FindIf([&](const Pair<String, uint64>& pair)
        {
            return pair.first == nodePath;
        });

        if (it == categories[Category].assets.End())
        {
            return false;
        }

        outEpoch = it->second;

        return true;
    }

    /*! \brief Get the current epoch revision for \p category */
    HYP_FORCE_INLINE uint64 GetEpochRev(int category) const
    {
        if (category < 0 || category >= int(categories.Size()))
        {
            return 0;
        }

        return categories[category].revision;
    }

    /*! \brief Bump the epoch revision for \p category.
     *  \returns Value after increment, or if \p category was OOB returns 0 */
    HYP_FORCE_INLINE uint64 BumpEpochRev(int category)
    {
        if (category < 0 || category >= int(categories.Size()))
        {
            return 0;
        }

        return ++categories[category].revision;
    }
};

} // namespace Baking

} // namespace Hyperion
