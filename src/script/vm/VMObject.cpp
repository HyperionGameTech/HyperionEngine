#include <script/vm/VMObject.hpp>
#include <script/vm/HeapValue.hpp>
#include <core/debug/Debug.hpp>
#include <core/math/MathUtil.hpp>
#include <core/HashCode.hpp>
#include <core/Core.hpp>

#include <cmath>

namespace hyperion {
namespace vm {

const HashCode::ValueType VMObject::PROTO_MEMBER_HASH = HashCode::GetHashCode("$proto").Value();
const HashCode::ValueType VMObject::BASE_MEMBER_HASH = HashCode::GetHashCode("base").Value();

ObjectMap::ObjectBucket::ObjectBucket()
    : m_data(new Member*[DEFAULT_BUCKET_CAPACITY]),
      m_capacity(DEFAULT_BUCKET_CAPACITY),
      m_size(0)
{
}

ObjectMap::ObjectBucket::ObjectBucket(const ObjectBucket& other)
    : m_data(new Member*[other.m_capacity]),
      m_capacity(other.m_capacity),
      m_size(other.m_size)
{
    Memory::MemCpy(
        m_data,
        other.m_data,
        sizeof(Member*) * other.m_size);
}

ObjectMap::ObjectBucket::~ObjectBucket()
{
    delete[] m_data;
}

ObjectMap::ObjectBucket& ObjectMap::ObjectBucket::operator=(const ObjectBucket& other)
{
    if (m_capacity < other.m_capacity)
    {
        // capacity must be resized
        delete[] m_data;
        m_data = new Member*[other.m_capacity];
        m_capacity = other.m_capacity;
    }

    // copy members
    Memory::MemCpy(
        m_data,
        other.m_data,
        sizeof(Member*) * other.m_size);

    return *this;
}

void ObjectMap::ObjectBucket::Resize(SizeType capacity)
{
    if (m_capacity < capacity)
    {
        const SizeType newCapacity = COMPUTE_CAPACITY(capacity);
        // create new bucket and copy
        Member** newData = new Member*[newCapacity];
        Memory::MemCpy(
            newData,
            m_data,
            sizeof(Member*) * m_size);
        m_capacity = newCapacity;
        delete[] m_data;
        m_data = newData;
    }
}

void ObjectMap::ObjectBucket::Push(Member* member)
{
    if (m_size >= m_capacity)
    {
        Resize(COMPUTE_CAPACITY(MathUtil::Max(m_capacity, m_size) + 1));
    }

    m_data[m_size++] = member;
}

bool ObjectMap::ObjectBucket::Lookup(HashCode::ValueType hash, Member** out)
{
    for (SizeType i = 0; i < m_size; i++)
    {
        if (m_data[i]->hash == hash)
        {
            *out = m_data[i];

            return true;
        }
    }

    return false;
}

ObjectMap::ObjectMap(SizeType size)
    : m_size(size)
{

    m_buckets = new ObjectMap::ObjectBucket[m_size];
}

ObjectMap::ObjectMap(const ObjectMap& other)
    : m_size(other.m_size)
{
    m_buckets = new ObjectMap::ObjectBucket[m_size];

    for (SizeType i = 0; i < m_size; i++)
    {
        m_buckets[i] = other.m_buckets[i];
    }
}

ObjectMap::~ObjectMap()
{
    delete[] m_buckets;
}

ObjectMap& ObjectMap::operator=(const ObjectMap& other)
{
    if (m_size != other.m_size)
    {
        // delete buckets and recreate with new size
        delete[] m_buckets;
        m_buckets = new ObjectMap::ObjectBucket[other.m_size];
    }

    m_size = other.m_size;

    for (SizeType i = 0; i < m_size; i++)
    {
        m_buckets[i] = other.m_buckets[i];
    }

    return *this;
}

void ObjectMap::Push(HashCode::ValueType hash, Member* member)
{
    Assert(m_size != 0);
    m_buckets[hash % m_size].Push(member);
}

Member* ObjectMap::Get(HashCode::ValueType hash)
{
    if (m_size == 0)
    {
        return nullptr;
    }

    Member* res = nullptr;
    m_buckets[hash % m_size].Lookup(hash, &res);

    return res;
}

VMObject::VMObject(const VMObject* prototype)
{
    Assert(prototype != nullptr);

    SizeType size = prototype->GetSize();

    m_members = new Member[size];
    Memory::MemCpy(m_members, prototype->GetMembers(), sizeof(Member) * size);

    m_objectMap = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++)
    {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(Value&& classValue)
    : m_classValue(std::move(classValue))
{
    const VMObject* protoObj = m_classValue.GetObject();
    Assert(protoObj != nullptr);

    SizeType size = protoObj->GetSize();

    m_members = new Member[size];
    Memory::MemCpy(m_members, protoObj->GetMembers(), sizeof(Member) * size);

    m_objectMap = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++)
    {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(const Member* members, SizeType size, Value&& classValue)
    : m_classValue(std::move(classValue))
{
    Assert(members != nullptr);

    m_members = new Member[size];
    Memory::MemCpy(m_members, members, sizeof(Member) * size);

    m_objectMap = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++)
    {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(VMObject&& other) noexcept
    : m_classValue(std::move(other.m_classValue)),
      m_objectMap(other.m_objectMap),
      m_members(other.m_members)
{
    other.m_objectMap = nullptr;
    other.m_members = nullptr;
}

VMObject& VMObject::operator=(VMObject&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    delete m_objectMap;
    delete[] m_members;

    m_classValue = std::move(other.m_classValue);
    m_objectMap = other.m_objectMap;
    m_members = other.m_members;

    other.m_objectMap = nullptr;
    other.m_members = nullptr;

    return *this;
}

VMObject::~VMObject()
{
    delete m_objectMap;
    m_objectMap = nullptr;

    delete[] m_members;
    m_members = nullptr;
}

Member* VMObject::LookupMemberFromHash(HashCode::ValueType hash, bool deep) const
{
    if (Member* member = m_objectMap->Get(hash))
    {
        return member;
    }

    if (deep && m_classValue.IsValid())
    {
        if (VMObject* baseObject = m_classValue.GetObject())
        {
            return baseObject->LookupMemberFromHash(hash);
        }
    }

    return nullptr;
}

void VMObject::SetMember(const char* name, Value&& value)
{
    const HashCode::ValueType hash = HashCode::GetHashCode(name).Value();

    Member* member = LookupMemberFromHash(hash, /* deep */ false);

    if (member)
    {
        member->value.AssignValue(std::move(value), false);

        return;
    }

    const SizeType prevSize = m_objectMap->GetSize();
    const SizeType newSize = prevSize + 1;

    Member* newMembers = new Member[newSize];
    Memory::MemCpy(newMembers, m_members, sizeof(Member) * prevSize);

    Memory::StrCpy(newMembers[newSize - 1].name, name, sizeof(newMembers[newSize - 1].name));
    newMembers[newSize - 1].hash = hash;
    newMembers[newSize - 1].value.AssignValue(std::move(value), false);

    ObjectMap* newObjectMap = new ObjectMap(newSize);
    for (SizeType i = 0; i < newSize; i++)
    {
        newObjectMap->Push(newMembers[i].hash, &newMembers[i]);
    }

    newObjectMap->Push(hash, &newMembers[newSize - 1]);

    delete[] m_members;
    delete m_objectMap;

    m_members = newMembers;
    m_objectMap = newObjectMap;
}

void VMObject::GetRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
{
    if (depth == 0)
    {
        ss << static_cast<const void*>(this);

        return;
    }

    const SizeType size = GetSize();

    ss << "{";

    for (SizeType i = 0; i < size; i++)
    {
        vm::Member& mem = m_members[i];

        ss << mem.name << ": ";

        if (mem.value.GetObject() == this)
        {
            mem.value.ToRepresentation(
                ss,
                addTypeName,
                0 // prevent circular reference causing infinite loop
            );
        }
        else
        {
            mem.value.ToRepresentation(
                ss,
                addTypeName,
                depth - 1);
        }

        if (i != size - 1)
        {
            ss << ", ";
        }
    }

    ss << "}";
}

} // namespace vm
} // namespace hyperion
