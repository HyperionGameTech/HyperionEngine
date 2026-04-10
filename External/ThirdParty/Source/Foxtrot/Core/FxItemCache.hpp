#pragma once

#include <Core/FxBitset.hpp>
#include <Core/FxHash.hpp>
#include <Core/FxSizedArray.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <map>
#include <unordered_map>

/**
 * @brief Cache section that can hold any number of items.
 */
template <typename TItemType>
struct FxItemCacheSection_MultiItem
{
    FxBitset ItemsInUse {};
    FxSizedArray<TItemType> Items {};

public:
    void Create(uint32 max_items)
    {
        Items.InitCapacity(max_items);
        ItemsInUse.InitZero(max_items);
    }

    TItemType* Request()
    {
        int index = ItemsInUse.FindNextFreeBit();

        if (index == FxBitset::scNoFreeBits) {
            return nullptr;
        }


        ItemsInUse.Set(index);

        FxLogDebug("Request item at index {}, preallocated capacity of {}", index, Items.Capacity);

        TItemType* item = &Items[index];

        FxLogDebug("After");

        return item;
    }

    void Release(TItemType* item)
    {
        auto item_index = Items.GetItemIndex(item);

        FxLogDebug("Release item");

        // If the item was found, clear the bit
        if (item_index != FxSizedArray<TItemType>::scItemNotFound) {
            ItemsInUse.Unset(item_index);
        }
        else {
            FxLogDebug("Could not find cache item!");
        }
    }
};


/**
 * @brief Cache section that holds a single item.
 */
template <typename TItemType>
struct FxItemCacheSection_SingleItem
{
    using ItemType = TItemType;

    TItemType* pItem = nullptr;
    bool bInUse : 1 = false;

public:
    void Create(uint32 max_items) { pItem = gEnginePool->Alloc<TItemType>(sizeof(TItemType)); }

    TItemType* Request()
    {
        if (bInUse) {
            return nullptr;
        }

        bInUse = true;

        return pItem;
    }

    void Release(TItemType* item)
    {
        if (pItem != item) {
            return;
        }

        bInUse = false;
    }
};

template <typename T, typename TItemType>
concept C_IsItemCacheSection =
    C_IsAnyBaseOf<T, FxItemCacheSection_MultiItem<TItemType>, FxItemCacheSection_SingleItem<TItemType>>;

template <typename TKeyType, typename TItemType, typename TCacheSectionType>
    requires C_IsItemCacheSection<TCacheSectionType, TItemType>
class FxItemCache
{
public:
    using KeyType = TKeyType;

public:
    FxItemCache() = default;

    void Create(uint32 max_items_per_section) { mItemsPerSection = max_items_per_section; }


    bool IsItemInCache(TKeyType key)
    {
        auto section_it = mCache.find(key);

        return (section_it == mCache.end());
    }

    TCacheSectionType* RequestCacheSection(TKeyType key)
    {
        auto section_it = mCache.find(key);

        if (section_it == mCache.end()) {
            CreateCacheSection(key);
            section_it = mCache.find(key);

            // Could not find the newly created cache section, return error
            if (section_it == mCache.end()) {
                return nullptr;
            }
        }

        TCacheSectionType& section = section_it->second;

        return &section;
    }

    TItemType* RequestGenericItem(TKeyType key)
    {
        auto section_it = mCache.find(key);

        if (section_it == mCache.end()) {
            CreateCacheSection(key);
            section_it = mCache.find(key);

            FxAssert(section_it != mCache.end());
        }

        FxLogInfo("Request new item!");

        TCacheSectionType& section = section_it->second;

        return section.Request();
    }

    void ReleaseGenericItem(TKeyType key, TItemType* item)
    {
        auto section_it = mCache.find(key);

        if (section_it == mCache.end()) {
            FxLogError("Could not find cache section to free from!");
            return;
        }

        TCacheSectionType& section = section_it->second;

        section.Release(item);

        // auto item_index = section.Items.GetItemIndex(item);

        // // If the item was found, clear the bit
        // if (item_index != FxSizedArray<TItemType>::scItemNotFound) {
        //     section.ItemsInUse.Unset(item_index);
        // }
    }


    virtual ~FxItemCache() {}

protected:
    TCacheSectionType& CreateCacheSection(TKeyType key)
    {
        FxLogDebug("Create Cache section! {}", mItemsPerSection);
        TCacheSectionType cache_section {};

        // cache_section.Create(mItemsPerSection);

        // cache_section.Items.InitCapacity(number_of_items);
        // cache_section.ItemsInUse.InitZero(number_of_items);

        auto iter = mCache.insert({ key, std::move(cache_section) });

        iter.first->second.Create(mItemsPerSection);

        return iter.first->second;
    }

public:
    std::unordered_map<TKeyType, TCacheSectionType> mCache;

protected:
    uint32 mItemsPerSection = 0;
};

/**
 * @brief A temporary handle that is available only for the current scope.
 */
template <typename TKeyType, typename TItemType, typename TCacheType>
struct FxItemCacheHandle
{
    using CacheType = TCacheType;
    using KeyType = TKeyType;

public:
    FxItemCacheHandle(TCacheType* cache, TItemType* item, TKeyType key) : Item(item), Key(key), mCache(cache) {}
    FxItemCacheHandle(const FxItemCacheHandle& other) = delete;
    FxItemCacheHandle(FxItemCacheHandle&& other)
    {
        Item = other.Item;
        Key = other.Key;

        mCache = other.mCache;

        other.Item = nullptr;
    }

    /**
     * @brief Releases the handle before the destructor is called.
     */
    FX_FORCE_INLINE void Release()
    {
        if (Item == nullptr) {
            return;
        }

        mCache->ReleaseGenericItem(Key, Item);
    }

    ~FxItemCacheHandle() { Release(); }

    TItemType* operator->() { return Item; }
    TItemType& operator*() { return *Item; }

public:
    TItemType* Item = nullptr;
    TKeyType Key;

private:
    CacheType* mCache;
};
