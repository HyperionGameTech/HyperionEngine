/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>

namespace hyperion::fbom {

FBOMArray::FBOMArray(const FBOMType& element_type)
    : m_element_type(element_type)
{
}

FBOMArray::FBOMArray(const FBOMType& element_type, const Array<FBOMData>& values)
    : m_element_type(element_type),
      m_values(values)
{
    if (m_values.Any())
    {
        if (m_element_type.IsPlaceholder() && !m_values[0].GetType().IsPlaceholder())
        {
            m_element_type = m_values[0].GetType();
        }

        for (const FBOMData& value : m_values)
        {
            AssertThrowMsg(
                value.GetType().IsOrExtends(m_element_type),
                "Cannot add element of type '%s' to Array with element type '%s'",
                value.GetType().name.Data(),
                m_element_type.name.Data());
        }
    }
}

FBOMArray::FBOMArray(const FBOMType& element_type, Array<FBOMData>&& values)
    : m_element_type(element_type),
      m_values(std::move(values))
{
    if (m_values.Any())
    {
        if (m_element_type.IsPlaceholder() && !m_values[0].GetType().IsPlaceholder())
        {
            m_element_type = m_values[0].GetType();
        }

        for (const FBOMData& value : m_values)
        {
            AssertThrowMsg(
                value.GetType().IsOrExtends(m_element_type),
                "Cannot add element of type '%s' to Array with element type '%s'",
                value.GetType().name.Data(),
                m_element_type.name.Data());
        }
    }
}

FBOMArray::FBOMArray(const FBOMArray& other)
    : m_element_type(other.m_element_type),
      m_values(other.m_values)
{
}

FBOMArray& FBOMArray::operator=(const FBOMArray& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_element_type = other.m_element_type;
    m_values = other.m_values;

    return *this;
}

FBOMArray::FBOMArray(FBOMArray&& other) noexcept
    : m_element_type(std::move(other.m_element_type)),
      m_values(std::move(other.m_values))
{
}

FBOMArray& FBOMArray::operator=(FBOMArray&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_element_type = std::move(other.m_element_type);
    m_values = std::move(other.m_values);

    return *this;
}

FBOMArray::~FBOMArray()
{
}

FBOMArray& FBOMArray::AddElement(const FBOMData& value)
{
    // If the element type is a placeholder, set it to the type of the value when first added
    if (m_element_type.IsPlaceholder() && !value.GetType().IsPlaceholder())
    {
        m_element_type = value.GetType();
    }

    AssertThrowMsg(
        value.GetType().Is(m_element_type),
        "Cannot add element of type '%s' to Array with element type '%s'",
        value.GetType().name.Data(),
        m_element_type.name.Data());

    m_values.PushBack(value);

    return *this;
}

FBOMArray& FBOMArray::AddElement(FBOMData&& value)
{
    // If the element type is a placeholder, set it to the type of the value when first added
    if (m_element_type.IsPlaceholder() && !value.GetType().IsPlaceholder())
    {
        m_element_type = value.GetType();
    }

    AssertThrowMsg(
        value.GetType().Is(m_element_type),
        "Cannot add element of type '%s' to Array with element type '%s'",
        value.GetType().name.Data(),
        m_element_type.name.Data());

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
    static const FBOMData default_value {};

    if (index >= m_values.Size())
    {
        return default_value;
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

FBOMResult FBOMArray::Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
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
            ss << value.ToString(deep);
        }
    }
    else
    {
        ss << m_values.Size();
    }

    ss << " ] ";

    return String(ss.str().data());
}

UniqueID FBOMArray::GetUniqueID() const
{
    return UniqueID(GetHashCode());
}

HashCode FBOMArray::GetHashCode() const
{
    HashCode hc;
    hc.Add(m_values.Size());
    hc.Add(m_values.GetHashCode());

    return hc;
}

} // namespace hyperion::fbom