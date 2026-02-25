/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/serialization/fbom/FBOMArray.hpp>
#include <Core/serialization/fbom/FBOMWriter.hpp>

#include <sstream>

namespace Hyperion::serialization {

FBOMArray::FBOMArray() = default;

FBOMArray::FBOMArray(const Array<FBOMData>& values)
    : m_values(values)
{
}

FBOMArray::FBOMArray(Array<FBOMData>&& values)
    : m_values(std::move(values))
{
}

FBOMArray::FBOMArray(const FBOMArray& other)
    : m_values(other.m_values)
{
}

FBOMArray& FBOMArray::operator=(const FBOMArray& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_values = other.m_values;

    return *this;
}

FBOMArray::FBOMArray(FBOMArray&& other) noexcept
    : m_values(std::move(other.m_values))
{
}

FBOMArray& FBOMArray::operator=(FBOMArray&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_values = std::move(other.m_values);

    return *this;
}

FBOMArray::~FBOMArray()
{
}

FBOMArray& FBOMArray::AddElement(const FBOMData& value)
{
    m_values.PushBack(value);

    return *this;
}

FBOMArray& FBOMArray::AddElement(FBOMData&& value)
{
    m_values.PushBack(std::move(value));

    return *this;
}

FBOMData& FBOMArray::GetElement(SizeType index)
{
    return const_cast<FBOMData&>(static_cast<const FBOMArray*>(this)->GetElement(index));
}

const FBOMData& FBOMArray::GetElement(SizeType index) const
{
    // invalid result
    static const FBOMData s_defaultValue {};

    if (index >= m_values.Size())
    {
        return s_defaultValue;
    }

    return m_values[index];
}

const FBOMData* FBOMArray::TryGetElement(SizeType index) const
{
    if (index >= m_values.Size())
    {
        return nullptr;
    }

    return &m_values[index];
}

FBOMResult FBOMArray::Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

String FBOMArray::ToString(bool deep) const
{
    std::stringstream ss;

    ss << "[ ";

    if (deep)
    {
        for (const FBOMData& value : m_values)
        {
            ss << *value.ToString(deep);
        }
    }
    else
    {
        ss << m_values.Size();
    }

    ss << " ] ";

    return String(ss.str().data());
}

UniqueId FBOMArray::GetUniqueID() const
{
    return UniqueId(GetHashCode());
}

HashCode FBOMArray::GetHashCode() const
{
    HashCode hc;
    hc.Add(m_values.Size());
    hc.Add(m_values.GetHashCode());

    return hc;
}

} // namespace Hyperion::serialization