/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Asset/AssetPath.hpp>

namespace Hyperion {

namespace Baking {

HYP_STRUCT()
struct BakerSceneCategory
{
    HYP_STRUCT_BODY(BakerSceneCategory);

    enum { LightProvider, LightReceiver, Max };

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "LastBakeEpoch", Serialize)
    uint64 epoch = 0;

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
    }

    template <int Category, class T>
    bool Add(const T& obj)
    {
        if (!obj.GetScene()) return false; // not attached to a scene (???)

        const String nodePath = obj.GetScene()->GetName().ToString() + '/' + String::Join(MapToArray(obj.GetDeepPath(), &Name::ToString), '/');

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

        const String nodePath = obj.GetScene()->GetName().ToString() + '/' + String::Join(MapToArray(obj.GetDeepPath(), &Name::ToString), '/');

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
};

} // namespace Baking

} // namespace Hyperion
