#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <script/vm/Value.hpp>
#include <script/vm/TypeInfo.hpp>

#include <sstream>
#include <cstdint>
#include <cmath>

#define DEFAULT_BUCKET_CAPACITY 4
#define COMPUTE_CAPACITY(size) 1 << (unsigned)std::ceil(std::log(size) / std::log(2.0))

namespace hyperion {
namespace vm {

struct Member {
    uint32_t hash;
    Value value;
};

class ObjectMap {
public:
    ObjectMap(size_t size);
    ObjectMap(const ObjectMap &other);
    ~ObjectMap();

    ObjectMap &operator=(const ObjectMap &other);

    void Push(uint32_t hash, Member *member);
    Member *Get(uint32_t hash);

private:
    struct ObjectBucket {
        Member **m_data;
        size_t m_capacity;
        size_t m_size;

        ObjectBucket();
        ObjectBucket(const ObjectBucket &other);
        ~ObjectBucket();

        ObjectBucket &operator=(const ObjectBucket &other);

        void Resize(size_t capacity);
        void Push(Member *member);
        bool Lookup(uint32_t hash, Member **out);
    };

    ObjectBucket *m_buckets;
    size_t m_size;
};

class Object {
public:
    Object(TypeInfo *type_ptr, const Value &type_ptr_value);
    Object(const Object &other);
    ~Object();

    // compare by memory address
    inline bool operator==(const Object &other) const { return this == &other; }

    inline Member *LookupMemberFromHash(uint32_t hash) const
        { return m_object_map->Get(hash); }
    inline Member *GetMembers() const
        { return m_members; }
    inline Member &GetMember(int index)
        { return m_members[index]; }
    inline const Member &GetMember(int index) const
        { return m_members[index]; }
    inline TypeInfo *GetTypePtr() const
        { return m_type_ptr; }
    inline Value &GetTypePtrValue()
        { return m_type_ptr_value; }
    inline const Value &GetTypePtrValue() const
        { return m_type_ptr_value; }
    
    void GetRepresentation(std::stringstream &ss, bool add_type_name = true) const;

private:
    TypeInfo *m_type_ptr;
    Value m_type_ptr_value;
    ObjectMap *m_object_map;
    Member *m_members;
};

} // namespace vm
} // namespace hyperion

#endif
