#pragma once

#include "FxPagedArray.hpp"
#include "FxTypes.hpp"

template <typename T>
concept C_IsIntType = C_IsAnyOf<T, int16, uint16, int32, uint32, int64, uint64>;

template <typename T>
concept C_IsFloatType = std::is_same_v<T, float32>;

template <typename T>
concept C_IsSerializable = C_IsIntType<T> || C_IsFloatType<T>;


struct FxSerializerValue
{
public:
    enum Type
    {
        eInt,
        eFloat,
    };

public:
    FxSerializerValue() = delete;

    template <typename T>
        requires C_IsIntType<T>
    explicit FxSerializerValue(T value) : Type(eInt), IntValue(value)
    {
    }

    template <typename T>
        requires C_IsFloatType<T>
    explicit FxSerializerValue(T value) : Type(eFloat), FloatValue(value)
    {
    }

public:
    Type Type;

    union
    {
        int64 IntValue;
        float32 FloatValue;
    };
};

class FxSerializer
{
public:
    FxSerializer() { Values.Create(16); }

    FxSerializer(const FxSerializer& other) = delete;


    template <typename T>
        requires C_IsSerializable<T>
    FxSerializer& operator<<(T value)
    {
        Values.Insert(FxSerializerValue { value });
        return *this;
    }

    template <typename T>
        requires C_IsSerializable<T>
    FxSerializer& Add(T& member)
    {
        if (bInWriteMode) {
            (*this) << member;
        }
        else {
            (*this) >> member;
        }

        return *this;
    }

    void SetWriteMode() { bInWriteMode = true; }
    void SetReadMode() { bInWriteMode = false; }

    template <typename T>
        requires C_IsSerializable<T>
    FxSerializer& operator>>(T& value)
    {
        if (WriteIndex > Values.Size()) {
            return *this;
        }

        FxSerializerValue& stored_value = Values[WriteIndex++];

        if constexpr (C_IsIntType<T>) {
            value = stored_value.IntValue;
        }
        else if constexpr (C_IsFloatType<T>) {
            value = stored_value.FloatValue;
        }

        return *this;
    }


    void Rewind() { WriteIndex = 0; }

public:
    int WriteIndex = 0;
    FxPagedArray<FxSerializerValue> Values;

    bool bInWriteMode = true;
};


#define FX_SERIALIZABLE_MEMBERS(...)                                                                                   \
    template <typename... TMemberTypes>                                                                                \
    void SerializerDefineRecursive(FxSerializer& s, TMemberTypes&... types)                                            \
    {                                                                                                                  \
        (s.Add(types), ...);                                                                                           \
    }                                                                                                                  \
    void SerializeNow(FxSerializer& s) { SerializerDefineRecursive(s, __VA_ARGS__); }                                  \
    void Serialize(FxSerializer& s)                                                                                    \
    {                                                                                                                  \
        s.SetWriteMode();                                                                                              \
        SerializeNow(s);                                                                                               \
    }                                                                                                                  \
    void Deserialize(FxSerializer& s)                                                                                  \
    {                                                                                                                  \
        s.SetReadMode();                                                                                               \
        SerializeNow(s);                                                                                               \
    }
