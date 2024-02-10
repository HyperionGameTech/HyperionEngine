#ifndef HYPERION_V2_CORE_CLASS_INFO_HPP
#define HYPERION_V2_CORE_CLASS_INFO_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <type_traits>
#include <mutex>

namespace hyperion {

class ClassInfoBase
{
public:
    virtual ~ClassInfoBase() = default;
};

struct RegisteredClass
{
    uint index = ~0u;

    HYP_FORCE_INLINE bool IsValid() const
        { return index != ~0u; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }
};

struct GlobalClassInfoTable
{
    static constexpr uint max_class_objects = 1024;

    HeapArray<RC<ClassInfoBase>, max_class_objects> class_objects;
    std::mutex mtx;
    uint index = 0;

    template <class ClassInfo>
    RegisteredClass Register()
    {
        std::lock_guard guard(mtx);

        AssertThrowMsg(index < max_class_objects, "Too many class objects registered");

        uint object_index = index++;
        class_objects[object_index].Reset(new ClassInfo);

        return { object_index };
    }
};

template <class ClassInfo>
struct ClassInstance
{
    RegisteredClass registered_class;

    ClassInstance()
    {
    }
};

template <class ClassName>
class ClassInfo : public ClassInfoBase
{
    using ClassNameSequence = typename ClassName::Sequence;

public:
    static constexpr const char *GetName() { return ClassNameSequence::Data(); }

    ClassInfo()
        : ClassInfoBase()
    {
        // class_fields.Set<decltype(*this)>({ });
    }

    ClassInfo(const ClassInfo &other) = delete;
    ClassInfo &operator=(const ClassInfo &other) = delete;

    ClassInfo(ClassInfo &&other) = delete;
    ClassInfo &operator=(ClassInfo &&other) noexcept = delete;

    virtual ~ClassInfo() = default;
};
} // namespace hyperion

#endif