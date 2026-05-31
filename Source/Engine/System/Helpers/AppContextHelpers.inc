#ifdef HYP_WINDOWS
template <>
struct BoxedValueHelperDecl<HWND>
{
};

template <>
struct BoxedValueHelper<HWND> : BoxedValueHelper<void*>
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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, HWND value) const
    {
        boxed.Set_Internal(static_cast<void*>(value));
    }
};
#endif