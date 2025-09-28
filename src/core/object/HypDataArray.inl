// Array<T, AllocatorType>

template <class T, class AllocatorType>
HypDataArray::HypDataArray(AsReferenceTag, Array<T, AllocatorType>& arr)
    : HypDataArray()
{
    pInternalArray = &arr;
    dtor = nullptr;
    copyCtor = nullptr;

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, const Array<T, AllocatorType>& arr)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, Array<T, AllocatorType>&& arr)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };
}

// FixedArray<T, Sz>

template <class T, SizeType Sz>
HypDataArray::HypDataArray(AsReferenceTag, FixedArray<T, Sz>& arr)
    : HypDataArray()
{
    pInternalArray = &arr;
    dtor = nullptr;

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, SizeType Sz>
HypDataArray::HypDataArray(AsCopyTag, const FixedArray<T, Sz>& arr)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, SizeType Sz>
HypDataArray::HypDataArray(AsCopyTag, FixedArray<T, Sz>&& arr)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);
        HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
        return AnyRef(arr[index]);
    };
    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

// HashSet<T>

template <class T, auto KeyByFunction, class AllocatorType>
HypDataArray::HypDataArray(AsReferenceTag, HashSet<T, KeyByFunction, AllocatorType>& set)
    : HypDataArray()
{
    pInternalArray = &set;
    dtor = nullptr;

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, auto KeyByFunction, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, const HashSet<T, KeyByFunction, AllocatorType>& set)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(set);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, auto KeyByFunction, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, HashSet<T, KeyByFunction, AllocatorType>&& set)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(std::move(set));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

// HashMap<K, V>

template <class K, class V, class AllocatorType>
HypDataArray::HypDataArray(AsReferenceTag, HashMap<K, V, AllocatorType>& map)
    : HypDataArray()
{
    pInternalArray = &map;
    dtor = nullptr;

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, const HashMap<K, V, AllocatorType>& map)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(map);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
HypDataArray::HypDataArray(AsCopyTag, HashMap<K, V, AllocatorType>&& map)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(std::move(map));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

// LinkedList<T>

template <class T>
HypDataArray::HypDataArray(AsReferenceTag, LinkedList<T>& list)
    : HypDataArray()
{
    pInternalArray = &list;
    dtor = nullptr;

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
HypDataArray::HypDataArray(AsCopyTag, const LinkedList<T>& list)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(list);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
HypDataArray::HypDataArray(AsCopyTag, LinkedList<T>&& list)
    : HypDataArray()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(std::move(list));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (is_hyp_data_v<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const HypDataArray& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}