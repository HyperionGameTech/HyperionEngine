#include "FxBitset.hpp"

uint32 FxBitset::Init(uint32 max_bits)
{
    // 0 0 1 1 1 1 1 1 -> 63 -> 3F
    const uint8 remainder = (max_bits & 0x3F);

    // Adjust the amount of bits to be a multiple of 64
    if (remainder != 0) {
        max_bits += (64 - remainder);
    }

    const uint32 ints_required = (max_bits >> 6);

    mBits.InitSize(ints_required);

    return ints_required;
}


void FxBitset::InitZero(uint32 max_bits)
{
    uint32 num_ints = Init(max_bits);

    for (uint32 i = 0; i < num_ints; i++) {
        mBits[i] = 0;
    }
}

void FxBitset::InitOne(uint32 max_bits)
{
    uint32 num_ints = Init(max_bits);

    for (uint32 i = 0; i < num_ints; i++) {
        mBits[i] = ~0ULL;
    }
}


// void FxBitset::Set(uint32 index)
// {
//     // The index into the int array (index / 64)
//     const uint16 int_index = (index >> 6);

//     // The mask for the bit that we are querying
//     const uint64 mask = (1ULL << (index & 0x3F));

//     mBits.Data[int_index] |= mask;
// }


static constexpr uint8 GetBit(uint8 byte, uint8 bit) { return ((byte >> bit) & 0x01); }

static void PrintByte(uint8 b)
{
    printf("%d%d%d%d %d%d%d%d ", GetBit(b, 7), GetBit(b, 6), GetBit(b, 5), GetBit(b, 4), GetBit(b, 3), GetBit(b, 2),
           GetBit(b, 1), GetBit(b, 0));
}


void FxBitset::Print()
{
    uint32 size = static_cast<uint32>(mBits.Size);

    for (int i = 0; i < size; i++) {
        uint64 value = mBits.pData[i];

        printf("Bits(%d): ", i + 1);

        PrintByte(static_cast<uint8>(value >> 56));
        PrintByte(static_cast<uint8>(value >> 48));
        PrintByte(static_cast<uint8>(value >> 40));
        PrintByte(static_cast<uint8>(value >> 32));

        PrintByte(static_cast<uint8>(value >> 24));
        PrintByte(static_cast<uint8>(value >> 16));
        PrintByte(static_cast<uint8>(value >> 8));
        PrintByte(static_cast<uint8>(value >> 0));

        printf("\n");
    }
}
