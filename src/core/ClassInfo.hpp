#ifndef HYPERION_V2_CORE_CLASS_INFO_HPP
#define HYPERION_V2_CORE_CLASS_INFO_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeMap.hpp>

#include <script/ScriptApi.hpp>
#include <script/vm/Value.hpp>

#include <type_traits>
#include <mutex>

namespace hyperion {

class ClassInfoBase
{
public:
    ClassInfoBase()
        : m_class_ptr(nullptr)
    {
    }

    virtual ~ClassInfoBase() = default;

    vm::HeapValue *GetScriptHeapValue() const
        { return m_class_ptr; }

    void SetScriptHeapValue(vm::HeapValue *ptr)
        { m_class_ptr = ptr; }

protected:
    vm::HeapValue *m_class_ptr;
};

struct RegisteredClass
{
    UInt index = ~0u;

    HYP_FORCE_INLINE bool IsValid() const
        { return index != ~0u; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    const RC<ClassInfoBase> &GetRefCounted() const;
};

struct GlobalClassInfoTable
{
    static constexpr UInt max_class_objects = 1024;

    HeapArray<RC<ClassInfoBase>, max_class_objects> class_objects;
    std::mutex mtx;
    UInt index = 0;

    template <class ClassInfo>
    RegisteredClass Register()
    {
        std::lock_guard guard(mtx);

        AssertThrowMsg(index < max_class_objects, "Too many class objects registered");

        UInt object_index = index++;
        class_objects[object_index].Reset(new ClassInfo);

        return { object_index };
    }
};

extern GlobalClassInfoTable g_global_class_info_table;

template <class ClassInfo>
struct ClassInstance
{
    RegisteredClass registered_class;

    ClassInstance()
    {
        registered_class = g_global_class_info_table.Register<ClassInfo>();
        AssertThrow(registered_class.IsValid());
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

    static const ClassInfo &GetInstance()
    {
        static ClassInstance<ClassInfo> instance;

        return *static_cast<ClassInfo *>(instance.registered_class.GetRefCounted().Get());
    }
};
} // namespace hyperion

#endif