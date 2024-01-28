#include <script/vm/VMObject.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/Hasher.hpp>
#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <core/Core.hpp>

#include <cmath>

namespace hyperion {
namespace vm {

const UInt32 VMObject::PROTO_MEMBER_HASH = hash_fnv_1("$proto");
const UInt32 VMObject::BASE_MEMBER_HASH = hash_fnv_1("base");

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
    Memory::MemCpy(
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
    Memory::MemCpy(
        m_data,
        other.m_data,
        sizeof(Member *) * other.m_size
    );

    return *this;
}

void ObjectMap::ObjectBucket::Resize(SizeType capacity)
{
    if (m_capacity < capacity) {
        const SizeType new_capacity = COMPUTE_CAPACITY(capacity);
        // create new bucket and copy
        Member **new_data = new Member*[new_capacity];
        Memory::MemCpy(
            new_data,
            m_data,
            sizeof(Member *) * m_size
        );
        m_capacity = new_capacity;
        delete[] m_data;
        m_data = new_data;
    }
}

void ObjectMap::ObjectBucket::Push(Member *member)
{
    if (m_size >= m_capacity) {
        Resize(COMPUTE_CAPACITY(MathUtil::Max(m_capacity, m_size) + 1));
    }

    m_data[m_size++] = member;
}

bool ObjectMap::ObjectBucket::Lookup(UInt32 hash, Member **out)
{
    for (SizeType i = 0; i < m_size; i++) {
        if (m_data[i]->hash == hash) {
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

ObjectMap::ObjectMap(const ObjectMap &other)
    : m_size(other.m_size)
{
    m_buckets = new ObjectMap::ObjectBucket[m_size];

    for (SizeType i = 0; i < m_size; i++) {
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

    for (SizeType i = 0; i < m_size; i++) {
        m_buckets[i] = other.m_buckets[i];
    }

    return *this;
}

void ObjectMap::Push(UInt32 hash, Member *member)
{
    AssertThrow(m_size != 0);
    m_buckets[hash % m_size].Push(member);
}

Member *ObjectMap::Get(UInt32 hash)
{
    if (m_size == 0) {
        return nullptr;
    }

    Member *res = nullptr;
    m_buckets[hash % m_size].Lookup(hash, &res);

    return res;
}

VMObject::VMObject(HeapValue *class_ptr)
    : m_class_ptr(class_ptr)
{
    AssertThrow(m_class_ptr != nullptr);

    const VMObject *proto_obj = m_class_ptr->GetPointer<VMObject>();
    AssertThrow(proto_obj != nullptr);

    SizeType size = proto_obj->GetSize();

    m_members = new Member[size];
    Memory::MemCpy(m_members, proto_obj->GetMembers(), sizeof(Member) * size);

    m_object_map = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(const Member *members, SizeType size, HeapValue *class_ptr)
    : m_class_ptr(class_ptr)
{
    AssertThrow(members != nullptr);

    m_members = new Member[size];
    Memory::MemCpy(m_members, members, sizeof(Member) * size);

    m_object_map = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(const VMObject &other)
    : m_class_ptr(other.m_class_ptr)
{
    const SizeType size = other.GetSize();

    m_members = new Member[size];

    AssertThrow(other.GetObjectMap() != nullptr);
    m_object_map = new ObjectMap(*other.GetObjectMap());

    Memory::MemCpy(m_members, other.m_members, sizeof(Member) * size);

    for (SizeType i = 0; i < size; i++) {
        m_object_map->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::~VMObject()
{
    m_class_ptr = nullptr;

    delete m_object_map;
    m_object_map = nullptr;

    delete[] m_members;
    m_members = nullptr;
}

Member *VMObject::LookupMemberFromHash(UInt32 hash, bool deep) const
{
    if (Member *member = m_object_map->Get(hash)) {
        return member;
    }

    if (deep && m_class_ptr != nullptr) {
        if (VMObject *base_object = m_class_ptr->GetPointer<VMObject>()) {
            return base_object->LookupMemberFromHash(hash);
        }
    }

    return nullptr;
}

void VMObject::SetMember(const char *name, const Value &value)
{
    const UInt32 hash = hash_fnv_1(name);
    
    Member *member = LookupMemberFromHash(hash, /* deep */ false);

    if (member) {
        member->value = value;

        return;
    }

    const SizeType prev_size = m_object_map->GetSize();
    const SizeType new_size = prev_size + 1;

    Member *new_members = new Member[new_size];
    Memory::MemCpy(new_members, m_members, sizeof(Member) * prev_size);

    Memory::StrCpy(new_members[new_size - 1].name, name, sizeof(new_members[new_size - 1].name));
    new_members[new_size - 1].hash = hash;
    new_members[new_size - 1].value = value;

    ObjectMap *new_object_map = new ObjectMap(new_size);
    for (SizeType i = 0; i < new_size; i++) {
        new_object_map->Push(new_members[i].hash, &new_members[i]);
    }

    new_object_map->Push(hash, &new_members[new_size - 1]);

    delete[] m_members;
    delete m_object_map;

    m_members = new_members;
    m_object_map = new_object_map;
}

void VMObject::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    if (depth == 0) {
        ss << static_cast<const void *>(this);

        return;
    }

    const SizeType size = GetSize();

    ss << "{";

    for (SizeType i = 0; i < size; i++) {
        vm::Member &mem = m_members[i];

        ss << mem.name << ": ";

        if (mem.value.m_type == Value::HEAP_POINTER &&
            mem.value.m_value.ptr != nullptr &&
            mem.value.m_value.ptr->GetRawPointer() == static_cast<const void *>(this))
        {
            mem.value.ToRepresentation(
                ss,
                add_type_name,
                0 // prevent circular reference causing infinite loop
            );
        } else {
            mem.value.ToRepresentation(
                ss,
                add_type_name,
                depth - 1
            );
        }

        if (i != size - 1) {
            ss << ", ";
        }
    }

    ss << "}";
}

HashCode VMObject::GetHashCode() const
{
    // Hash of memory address

    // @NOTE: If we ever implement a generational garbage collector,
    // we'll need to change this to something else. Perhaps
    // we could use a hash of an ID that is unique to each
    // object.

    return HashCode::GetHashCode(static_cast<const void *>(this));
}

} // namespace hyperion
} // namespace vm
