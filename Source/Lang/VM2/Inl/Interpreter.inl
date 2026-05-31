#define HYP_NUMERIC_OPERATION(a, b, oper)                                         \
    do                                                                            \
    {                                                                             \
        switch (numericType)                                                      \
        {                                                                         \
        case NT_I8:                                                               \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int8>(a.u);                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int8>(b.u);                         \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;          \
            }                                                                     \
            break;                                                                \
        case NT_I16:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int16>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int16>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;         \
            }                                                                     \
            break;                                                                \
        case NT_I32:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int32>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int32>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;         \
            }                                                                     \
            break;                                                                \
        case NT_I64:                                                              \
            if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i = static_cast<int64>(a.u);                               \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i = a.i;                                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            if (b.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                     \
                result.i oper## = static_cast<int64>(b.u);                        \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.i oper## = b.i;                                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;         \
            }                                                                     \
            break;                                                                \
        case NT_U8:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint8>(a.i);                               \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint8>(a.u);                               \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint8>(b.i);                        \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint8>(b.u);                        \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;        \
            }                                                                     \
            break;                                                                \
        case NT_U16:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint16>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint16>(a.u);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint16>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint16>(b.u);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_U32:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint32>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = static_cast<uint32>(a.u);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint32>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = static_cast<uint32>(b.u);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_U64:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u = static_cast<uint64>(a.i);                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u = a.u;                                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.u oper## = static_cast<uint64>(b.i);                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.u oper## = b.u;                                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;       \
            }                                                                     \
            break;                                                                \
        case NT_F32:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f = static_cast<float>(a.i);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f = static_cast<float>(a.u);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f = static_cast<float>(a.f);                               \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f oper## = static_cast<float>(b.i);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f oper## = static_cast<float>(b.u);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f oper## = static_cast<float>(b.f);                        \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT; \
            }                                                                     \
            break;                                                                \
        case NT_F64:                                                              \
            if (a.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f = static_cast<double>(a.i);                              \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f = static_cast<double>(a.u);                              \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f = a.f;                                                   \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            if (b.flags & Number::FLAG_SIGNED)                                    \
            {                                                                     \
                result.f oper## = static_cast<double>(b.i);                       \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else if (a.flags & Number::FLAG_UNSIGNED)                             \
            {                                                                     \
                result.f oper## = static_cast<double>(b.u);                       \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                result.f oper## = b.f;                                            \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT; \
            }                                                                     \
            break;                                                                \
        default:                                                                  \
            Assert(false, "Invalid type, should not reach this state.");          \
            break;                                                                \
        }                                                                         \
    }                                                                             \
    while (0)

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper)                                     \
    do                                                                                \
    {                                                                                 \
        switch (numericType)                                                          \
        {                                                                             \
        case NT_I8:                                                                   \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int8>(a.u);                                    \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int8>(b.u);                             \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;              \
            }                                                                         \
            break;                                                                    \
        case NT_I16:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int16>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int16>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_I32:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int32>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int32>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_I64:                                                                  \
            if (a.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i = static_cast<int64>(a.u);                                   \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i = a.i;                                                       \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            if (b.flags & Number::FLAG_UNSIGNED)                                      \
            {                                                                         \
                result.i oper## = static_cast<int64>(b.u);                            \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.i oper## = b.i;                                                \
                result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;             \
            }                                                                         \
            break;                                                                    \
        case NT_U8:                                                                   \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint8>(a.i);                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint8>(a.u);                                   \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint8>(b.i);                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint8>(b.u);                            \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;            \
            }                                                                         \
            break;                                                                    \
        case NT_U16:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint16>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint16>(a.u);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint16>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint16>(b.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;           \
            }                                                                         \
            break;                                                                    \
        case NT_U32:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint32>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = static_cast<uint32>(a.u);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint32>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = static_cast<uint32>(b.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;           \
            }                                                                         \
            break;                                                                    \
        case NT_U64:                                                                  \
            if (a.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u = static_cast<uint64>(a.i);                                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u = a.u;                                                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            if (b.flags & Number::FLAG_SIGNED)                                        \
            {                                                                         \
                result.u oper## = static_cast<uint64>(b.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            else                                                                      \
            {                                                                         \
                result.u oper## = b.u;                                                \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;           \
            }                                                                         \
            break;                                                                    \
        default:                                                                      \
            vm->ThrowException(instance, Exception::InvalidBitwiseArgument()); \
            break;                                                                    \
        }                                                                             \
    }                                                                                 \
    while (0)