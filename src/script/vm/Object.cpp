#include <script/vm/Object.hpp>
#include <script/vm/HeapValue.hpp>

#include <system/debug.h>
#include <script/Hasher.hpp>

#include <cstring>
#include <cmath>

namespace hyperion {
namespace vm {

const uint32_t Object::PROTO_MEMBER_HASH = hash_fnv_1("$proto");

ObjectMap::ObjectBucket::ObjectBucket()
    : m_data(new Member*[DEFAULT_BUCKET_CAPACITY]),
      m_capacity(DEFAULT_BUCKET_CAPACITY),
      m_size(0)
{
}

ObjectMap::ObjectBucket::ObjectBucket(const ObjectBucket &other)
    : m_data(new Member*[other.m_capacity]),
      m_capacity(other.m_capacity),
      m_size(other.m_size)
{
    std::memcpy(
        m_data,
        other.m_data,
        sizeof(Member *) * other.m_size
    );
}

ObjectMap::ObjectBucket::~ObjectBucket()
{
    delete[] m_data;
}

ObjectMap::ObjectBucket &ObjectMap::ObjectBucket::operator=(const ObjectBucket &other)
{
    if (m_capacity < other.m_capacity) {
        // capacity must be resized
        delete[] m_data;
        m_data = new Member*[other.m_capacity];
        m_capacity = other.m_capacity;
    }

    // copy members
    std::memcpy(
        m_data,
        other.m_data,
        sizeof(Member*) * other.m_size
    );

    return *this;
}

void ObjectMap::ObjectBucket::Resize(size_t capacity)
{
    if (m_capacity < capacity) {
        const size_t new_capacity = COMPUTE_CAPACITY(capacity);
        // create new bucket and copy
        Member **new_data = new Member*[new_capacity];
        std::memcpy(
            new_data,
            m_data,
            sizeof(Member*) * m_size
        );
        m_capacity = new_capacity;
        delete[] m_data;
        m_data = new_data;
    }
}

void ObjectMap::ObjectBucket::Push(Member *member)
{
    if (m_size == m_capacity) {
        Resize(COMPUTE_CAPACITY(m_capacity));
    }
    m_data[m_size++] = member;
}

bool ObjectMap::ObjectBucket::Lookup(uint32_t hash, Member **out)
{
    for (size_t i = 0; i < m_size; i++) {
        if (m_data[i]->hash == hash) {
            *out = m_data[i];
            return true;
        }
    }
    return false;
}


ObjectMap::ObjectMap(size_t size)
    : m_size(size)
{
    
    m_buckets = new ObjectMap::ObjectBucket[m_size];
}

ObjectMap::ObjectMap(const ObjectMap &other)
    : m_size(other.m_size)
{
    m_buckets = new ObjectMap::ObjectBucket[m_size];

    for (size_t i = 0; i < m_size; i++) {
        m_buckets[i] = other.m_buckets[i];
    }
}

ObjectMap::~ObjectMap()
{
    delete[] m_buckets;
}

ObjectMap &ObjectMap::operator=(const ObjectMap &other)
{
    if (m_size != other.m_size) {
        // delete buckets and recreate with new size
        delete[] m_buckets;
        m_buckets = new ObjectMap::ObjectBucket[other.m_size];
    }

    m_size = other.m_size;

    for (size_t i = 0; i < m_size; i++) {
        m_buckets[i] = other.m_buckets[i];
    }

    return *this;
}

void ObjectMap::Push(uint32_t hash, Member *member)
{
    AssertThrow(m_size != 0);
    m_buckets[hash % m_size].Push(member);
}

Member *ObjectMap::Get(uint32_t hash)
{
    if (m_size == 0) {
        return nullptr;
    }

    Member *res = nullptr;
    m_buckets[hash % m_size].Lookup(hash, &res);

    return res;
}

Object::Object(HeapValue *proto)
    : m_proto(proto)
{
    AssertThrow(proto != nullptr);

    const Object *proto_obj = proto->GetPointer<Object>();
    AssertThrow(proto_obj != nullptr);

    size_t size = proto_obj->GetSize();

    // auto **names = m_type_ptr->GetNames();
    // AssertThrow(names != nullptr);

    m_members = new Member[size];
    std::memcpy(m_members, proto_obj->GetMembers(), sizeof(Member) * size);

    m_object_map = new ObjectMap(size);
    for (size_t i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }


    /*// compute hash for member name
    uint32_t hash = hash_fnv_1(names[i]);
    m_members[i].hash = hash;
    
    m_object_map->Push(hash, &m_members[i]);*/
}

Object::Object(const Member *members, size_t size, HeapValue *proto)
    : m_proto(proto)
{
    AssertThrow(members != nullptr);

    m_members = new Member[size];
    std::memcpy(m_members, members, sizeof(Member) * size);

    m_object_map = new ObjectMap(size);
    for (size_t i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }
}

Object::Object(const Object &other)
    : m_proto(other.m_proto)
{
    const size_t size = other.GetSize();

    m_members = new Member[size];

    AssertThrow(other.GetObjectMap() != nullptr);
    m_object_map = new ObjectMap(*other.GetObjectMap());

    std::memcpy(m_members, other.m_members, sizeof(Member) * size);

    for (size_t i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }
}

Object::~Object()
{
    m_proto = nullptr;

    delete m_object_map;
    m_object_map = nullptr;

    delete[] m_members;
    m_members = nullptr;
}

void Object::GetRepresentation(std::stringstream &ss, bool add_type_name) const
{
    const size_t size = GetSize();

    ss << "{ ";

    for (size_t i = 0; i < size; i++) {
        vm::Member &mem = m_members[i];

        ss << mem.name << ": ";

        if (mem.value.m_type == Value::HEAP_POINTER &&
            mem.value.m_value.ptr != nullptr &&
            mem.value.m_value.ptr->GetRawPointer<void>() == (void*)this) {
            ss << "<circular reference>";
        } else {
            mem.value.ToRepresentation(ss, add_type_name);
        }

        if (i != size - 1) {
            ss << ", ";
        }
    }

    ss << " }";
}

} // namespace vm
} // namespace hyperion
