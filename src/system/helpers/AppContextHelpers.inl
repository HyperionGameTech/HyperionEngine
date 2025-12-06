#ifdef HYP_WINDOWS
template <>
struct HypDataHelperDecl<HWND>
{
};

template <>
struct HypDataHelper<HWND> : HypDataHelper<void*>
{
    HYP_FORCE_INLINE bool Is(HWND value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE bool Is(void* value) const
    {
        return true;
    }

    HYP_FORCE_INLINE HWND Get(void* value) const
    {
        return static_cast<HWND>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, HWND value) const
    {
        hypData.Set_Internal(static_cast<void*>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(HWND value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Serialization of HWND is not supported." };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        return { FBOMResult::FBOM_ERR, "Deserialization of HWND is not supported." };
    }
};
#endif