/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct SkeletonBindData;

HYP_STRUCT()
struct BoneMaskEntry
{
    HYP_STRUCT_BODY(BoneMaskEntry);

    HYP_FIELD(Property = "BoneName", Serialize = true, Editor = true, EditorType = "Bone")
    Name boneName;

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    HYP_FIELD(Property = "IncludeDescendants", Serialize = true, Editor = true)
    bool includeDescendants = true;

    HYP_FORCE_INLINE bool operator==(const BoneMaskEntry& other) const
    {
        return boneName == other.boneName && weight == other.weight && includeDescendants == other.includeDescendants;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(boneName);
        hc.Add(weight);
        hc.Add(includeDescendants);

        return hc;
    }
};

// per-bone weights for a layered blend; entries apply in order, so a later one overrides an earlier one's subtree
HYP_CLASS(AssetBucket = "Animations")
class ENGINE_API BoneMask final : public AssetObject
{
    HYP_OBJECT_BODY(BoneMask);

public:
    BoneMask();
    explicit BoneMask(Name name);
    ~BoneMask() override;

    HYP_METHOD(Property = "DefaultWeight")
    float GetDefaultWeight() const
    {
        return m_defaultWeight;
    }

    HYP_METHOD(Property = "DefaultWeight")
    void SetDefaultWeight(float defaultWeight)
    {
        m_defaultWeight = defaultWeight;
    }

    HYP_METHOD(Property = "Entries", NoScriptBindings)
    const Array<BoneMaskEntry>& GetEntries() const
    {
        return m_entries;
    }

    HYP_METHOD(Property = "Entries", NoScriptBindings)
    void SetEntries(const Array<BoneMaskEntry>& entries)
    {
        m_entries = entries;
    }

    Array<float> BuildSlotWeights(const SkeletonBindData& bindData) const;

    static Array<float> BuildExcludingSubtree(const SkeletonBindData& bindData, Name excludedBoneName);

private:
    HYP_FIELD(Property = "DefaultWeight")
    float m_defaultWeight = 0.0f;

    HYP_FIELD(Property = "Entries")
    Array<BoneMaskEntry> m_entries;
};

} // namespace Hyperion
