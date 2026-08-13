inline bool IsValidController(ControllerHandle handle)
{
    uint64 value = reinterpret_cast<uint64>(handle);
    return (value & 0xFF) != 0;
}

inline bool IsSteamInput(ControllerHandle handle)
{
    uint64 value = reinterpret_cast<uint64>(handle);
    return (value & 0x100) != 0;
}

inline uint8 GetControllerIndex(ControllerHandle handle)
{
    uint64 value = reinterpret_cast<uint64>(handle);
    return uint8(ByteUtil::LowestSetBitIndex(value & 0xFF));
}