#pragma once

#include <script/vm/Value.hpp>
#include <core/math/MathUtil.hpp>
#include <core/containers/Array.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <core/debug/Debug.hpp>

#include <sstream>

namespace hyperion {

class VMArray
{
public:
    explicit VMArray(SizeType size = 0);
    VMArray(const VMArray& other) = default;
    VMArray& operator=(const VMArray& other) = default;
    VMArray(VMArray&& other) noexcept = default;
    VMArray& operator=(VMArray&& other) noexcept = default;
    ~VMArray() = default;

    bool operator==(const VMArray& other) const
    {
        return m_internalArray == other.m_internalArray;
    }

    SizeType GetSize() const
    {
        return m_internalArray.Size();
    }

    Value* GetBuffer()
    {
        return m_internalArray.Data();
    }

    const Value* GetBuffer() const
    {
        return m_internalArray.Data();
    }

    Value& AtIndex(SizeType index)
    {
        return m_internalArray[index];
    }

    const Value& AtIndex(SizeType index) const
    {
        return m_internalArray[index];
    }

    void AtIndex(SizeType index, Value&& value)
    {
        m_internalArray[index] = std::move(value);
    }

    void Resize(SizeType newSize);
    void Push(Value&& value);
    void Pop();

    void GetRepresentation(
        std::stringstream& ss,
        bool addTypeName = true,
        int depth = 3) const;

private:
    static SizeType GetCapacityForSize(SizeType newSize)
    {
        return static_cast<SizeType>(1) << static_cast<SizeType>(std::ceil(std::log(MathUtil::Max(newSize, 1)) / std::log(2.0)));
    }

    Array<Value> m_internalArray;
};

} // namespace hyperion
