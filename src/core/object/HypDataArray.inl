// Array<T, AllocatorType>

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, Array<T, AllocatorType>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = &arr;
    dtor = nullptr;
    copyCtor = nullptr;

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            out = arr[index];
        }
        else
        {
            out = HypData(arr[index]);
        }

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, SizeType newSize) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        arr.Resize(newSize);

        return true;
    };
}

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const Array<T, AllocatorType>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            out = arr[index];
        }
        else
        {
            out = HypData(arr[index]);
        }

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, SizeType newSize) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        arr.Resize(newSize);

        return true;
    };
}

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, Array<T, AllocatorType>&& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const Array<T, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<Array<T, AllocatorType>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            out = arr[index];
        }
        else
        {
            out = HypData(arr[index]);
        }

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, SizeType newSize) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        arr.Resize(newSize);

        return true;
    };
}

// FixedArray<T, Sz>

template <class T, SizeType Sz>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, FixedArray<T, Sz>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = &arr;
    dtor = nullptr;

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        out = HypData(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, SizeType Sz>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const FixedArray<T, Sz>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        out = HypData(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, SizeType Sz>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, FixedArray<T, Sz>&& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return HypDataHelper<FixedArray<T, Sz>>::Serialize(arr, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(arr)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, SizeType index, HypData& out) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        out = HypData(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, SizeType index, HypData&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsHypDataV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

// HashSet<T>

template <class T, auto KeyByFunction, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, HashSet<T, KeyByFunction, AllocatorType>& set)
    : GenericArrayWrapper()
{
    pInternalArray = &set;
    dtor = nullptr;

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, auto KeyByFunction, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const HashSet<T, KeyByFunction, AllocatorType>& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(set);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, auto KeyByFunction, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, HashSet<T, KeyByFunction, AllocatorType>&& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(std::move(set));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(hashSet, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(set)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashSet = *static_cast<const HashSet<T, KeyByFunction, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

// HashMap<K, V>

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, HashMap<K, V, AllocatorType>& map)
    : GenericArrayWrapper()
{
    pInternalArray = &map;
    dtor = nullptr;

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const HashMap<K, V, AllocatorType>& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(map);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, HashMap<K, V, AllocatorType>&& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(std::move(map));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(hashMap, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(map)>>();
    elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& hashMap = *static_cast<const HashMap<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

// LinkedList<T>

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, LinkedList<T>& list)
    : GenericArrayWrapper()
{
    pInternalArray = &list;
    dtor = nullptr;

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const LinkedList<T>& list)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(list);
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, LinkedList<T>&& list)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(std::move(list));
    dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    serializeFunction = [](const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return HypDataHelper<LinkedList<T>>::Serialize(linkedList, outData, flags);
    };

    arrayTypeId = TypeId::ForType<std::remove_cvref_t<decltype(list)>>();
    elementTypeId = TypeId::ForType<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, HypData&& value) -> AnyRef
    {
        auto& list = *static_cast<LinkedList<T>*>(array.pInternalArray);

        if constexpr (IsHypDataV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> SizeType
    {
        auto& linkedList = *static_cast<const LinkedList<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}