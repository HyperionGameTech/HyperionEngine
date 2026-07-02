// Array<T, AllocatorType>

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, Array<T, AllocatorType>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = &arr;
    functionTable.dtor = nullptr;
    functionTable.copyCtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
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
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
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
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
    {
        auto& arr = *static_cast<Array<T, AllocatorType>*>(array.pInternalArray);
        arr.Resize(newSize);

        return true;
    };
}

// FixedArray<T, Sz>

template <class T, size_t Sz>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, FixedArray<T, Sz>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = &arr;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, size_t Sz>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const FixedArray<T, Sz>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

template <class T, size_t Sz>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, FixedArray<T, Sz>&& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<FixedArray<T, Sz>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<T>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<T>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<const FixedArray<T, Sz>*>(array.pInternalArray);
        return arr.Size();
    };
}

// Set<T>

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, Set<T, AllocatorType>& set)
    : GenericArrayWrapper()
{
    pInternalArray = &set;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashSet = *static_cast<const Set<T, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const Set<T, AllocatorType>& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(set);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashSet = *static_cast<const Set<T, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

template <class T, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, Set<T, AllocatorType>&& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(std::move(set));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashSet = *static_cast<const Set<T, AllocatorType>*>(array.pInternalArray);
        return hashSet.Size();
    };
}

// FlatSet<T>

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, FlatSet<T>& set)
    : GenericArrayWrapper()
{
    pInternalArray = &set;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatSet = *static_cast<const FlatSet<T>*>(array.pInternalArray);
        return flatSet.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const FlatSet<T>& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(set);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatSet = *static_cast<const FlatSet<T>*>(array.pInternalArray);
        return flatSet.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, FlatSet<T>&& set)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(set)>(std::move(set));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(set)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(set)>(*(std::remove_cvref_t<decltype(set)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(set)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatSet = *static_cast<const FlatSet<T>*>(array.pInternalArray);
        return flatSet.Size();
    };
}

// FlatMap<K, V>

template <class K, class V>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, FlatMap<K, V>& map)
    : GenericArrayWrapper()
{
    pInternalArray = &map;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatMap = *static_cast<const FlatMap<K, V>*>(array.pInternalArray);
        return flatMap.Size();
    };
}

template <class K, class V>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const FlatMap<K, V>& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(map);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatMap = *static_cast<const FlatMap<K, V>*>(array.pInternalArray);
        return flatMap.Size();
    };
}

template <class K, class V>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, FlatMap<K, V>&& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(std::move(map));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& flatMap = *static_cast<const FlatMap<K, V>*>(array.pInternalArray);
        return flatMap.Size();
    };
}

// Map<K, V>

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, Map<K, V, AllocatorType>& map)
    : GenericArrayWrapper()
{
    pInternalArray = &map;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashMap = *static_cast<const Map<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const Map<K, V, AllocatorType>& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(map);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashMap = *static_cast<const Map<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

template <class K, class V, class AllocatorType>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, Map<K, V, AllocatorType>&& map)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(map)>(std::move(map));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(map)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(map)>(*(std::remove_cvref_t<decltype(map)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(map)>>();
    elementTypeInfo = &TypeOf<KeyValuePair<K, V>>();

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& hashMap = *static_cast<const Map<K, V, AllocatorType>*>(array.pInternalArray);
        return hashMap.Size();
    };
}

// List<T>

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, List<T>& list)
    : GenericArrayWrapper()
{
    pInternalArray = &list;
    functionTable.dtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(list)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& list = *static_cast<List<T>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& linkedList = *static_cast<const List<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const List<T>& list)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(list);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(list)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& list = *static_cast<List<T>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& linkedList = *static_cast<const List<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

template <class T>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, List<T>&& list)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(list)>(std::move(list));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(list)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(list)>(*(std::remove_cvref_t<decltype(list)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(list)>>();
    elementTypeInfo = &TypeOf<T>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& list = *static_cast<List<T>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<T>)
        {
            return AnyRef(list.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        }
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& linkedList = *static_cast<const List<T>*>(array.pInternalArray);
        return linkedList.Size();
    };
}

#if !defined(HYP_USE_SLIM_ARRAY) || !HYP_USE_SLIM_ARRAY

// SlimArray<TElemType, TAllocator>

template <class TElemType, class TAllocator>
GenericArrayWrapper::GenericArrayWrapper(AsReferenceTag, SlimArray<TElemType, TAllocator>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = &arr;
    functionTable.dtor = nullptr;
    functionTable.copyCtor = nullptr;

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<TElemType>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<TElemType>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<TElemType>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<TElemType>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<TElemType>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        arr.Resize(static_cast<uint32>(newSize));

        return true;
    };
}

template <class TElemType, class TAllocator>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, const SlimArray<TElemType, TAllocator>& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(arr);
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<TElemType>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<TElemType>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<TElemType>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<TElemType>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<TElemType>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        arr.Resize(static_cast<uint32>(newSize));

        return true;
    };
}

template <class TElemType, class TAllocator>
GenericArrayWrapper::GenericArrayWrapper(AsCopyTag, SlimArray<TElemType, TAllocator>&& arr)
    : GenericArrayWrapper()
{
    pInternalArray = new std::remove_cvref_t<decltype(arr)>(std::move(arr));
    functionTable.dtor = &Memory::Delete<std::remove_cvref_t<decltype(arr)>>;
    functionTable.copyCtor = [](void** pDst, void* src)
    {
        *pDst = new std::remove_cvref_t<decltype(arr)>(*(std::remove_cvref_t<decltype(arr)>*)src);
    };

    typeInfo = &TypeOf<std::remove_cvref_t<decltype(arr)>>();
    elementTypeInfo = &TypeOf<TElemType>();

    functionTable.pushBack = [](GenericArrayWrapper& array, BoxedValue&& value) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if constexpr (IsBoxedValueV<TElemType>)
        {
            return AnyRef(arr.PushBack(std::move(value)));
        }
        else
        {
            return AnyRef(arr.PushBack(std::move(value.Get<TElemType>())));
        }
    };

    functionTable.getElementAt = [](GenericArrayWrapper& array, size_t index) -> AnyRef
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return AnyRef();
        }

        return AnyRef(&arr[index]);
    };

    functionTable.getElementAt2 = [](GenericArrayWrapper& array, size_t index, BoxedValue& outValue) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        outValue = BoxedValue(arr[index]);

        return true;
    };

    functionTable.setElementAt = [](GenericArrayWrapper& array, size_t index, BoxedValue&& value) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);

        if (index >= arr.Size())
        {
            return false;
        }

        if constexpr (IsBoxedValueV<TElemType>)
        {
            arr[index] = std::move(value);
        }
        else
        {
            arr[index] = value.Get<TElemType>();
        }

        return true;
    };

    functionTable.size = [](const GenericArrayWrapper& array) -> size_t
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        return arr.Size();
    };

    functionTable.resize = [](GenericArrayWrapper& array, size_t newSize) -> bool
    {
        auto& arr = *static_cast<SlimArray<TElemType, TAllocator>*>(array.pInternalArray);
        arr.Resize(static_cast<uint32>(newSize));

        return true;
    };
}

#endif // !HYP_USE_SLIM_ARRAY
