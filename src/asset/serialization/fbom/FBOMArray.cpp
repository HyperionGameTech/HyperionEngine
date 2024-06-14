/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

namespace hyperion::fbom {

FBOMArray::FBOMArray()
{
}

FBOMArray::FBOMArray(const Array<FBOMData> &values)
    : m_values(values)
{
}

FBOMArray::FBOMArray(Array<FBOMData> &&values)
    : m_values(std::move(values))
{
}

FBOMArray::FBOMArray(const FBOMArray &other)
    : m_values(other.m_values)
{
}

FBOMArray &FBOMArray::operator=(const FBOMArray &other)
{
    if (this == &other) {
        return *this;
    }

    m_values = other.m_values;

    return *this;
}

FBOMArray::FBOMArray(FBOMArray &&other) noexcept
    : m_values(std::move(other.m_values))
{
}

FBOMArray &FBOMArray::operator=(FBOMArray &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_values = std::move(other.m_values);

    return *this;
}

FBOMArray::~FBOMArray()
{
}

FBOMArray &FBOMArray::AddElement(const FBOMData &value)
{
    m_values.PushBack(value);

    return *this;
}

FBOMArray &FBOMArray::AddElement(FBOMData &&value)
{
    m_values.PushBack(std::move(value));

    return *this;
}

const FBOMData &FBOMArray::GetElement(SizeType index) const
{
    // invalid result
    static const FBOMData default_value { };

    if (index >= m_values.Size()) {
        return default_value;
    }

    return m_values[index];
}

const FBOMData *FBOMArray::TryGetElement(SizeType index) const
{
    if (index >= m_values.Size()) {
        return nullptr;
    }

    return &m_values[index];
}

FBOMResult FBOMArray::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out) const
{
    return writer->Write(out, *this, id);
}

String FBOMArray::ToString(bool deep) const
{
    std::stringstream ss;

    ss << "[ ";

    if (deep) {
        for (const FBOMData &value : m_values) {
            ss << value.ToString(deep);
        }
    } else {
        ss << m_values.Size();
    }

    ss << " ] ";

    return String(ss.str().data());
}

} // namespace hyperion::fbom