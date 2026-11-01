inline bool IsValidController(ControllerId handle)
{
    uint32 value = uint32(handle);
    return (value & 0xFF) != 0;
}

inline bool IsSteamInput(ControllerId handle)
{
    uint32 value = uint32(handle);
    return (value & 0x100) != 0;
}

inline uint8 GetControllerIndex(ControllerId handle)
{
    uint32 value = uint32(handle);
    return uint8(ByteUtil::LowestSetBitIndex(value & 0xFF));
}