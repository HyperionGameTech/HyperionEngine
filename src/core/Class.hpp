#ifndef HYPERION_V2_CORE_CLASS_HPP
#define HYPERION_V2_CORE_CLASS_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeMap.hpp>

#include <script/ScriptApi.hpp>
#include <script/vm/Value.hpp>

#include <type_traits>
#include <mutex>

namespace hyperion {

class ClassBase
{
public:
    ClassBase()
        : m_class_ptr(nullptr)
    {
    }

    virtual ~ClassBase() = default;

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

    const RC<ClassBase> &GetRefCounted() const;
};

struct GlobalClassTable
{
    static constexpr UInt max_class_objects = 1024;

    HeapArray<RC<ClassBase>, max_class_objects> class_objects;
    std::mutex mtx;
    UInt index = 0;

    template <class Class>
    RegisteredClass Register()
    {
        std::lock_guard guard(mtx);

        AssertThrowMsg(index < max_class_objects, "Too many class objects registered");

        UInt object_index = index++;
        class_objects[object_index].Reset(new Class);

        return { object_index };
    }
};

extern GlobalClassTable g_global_class_table;

template <class Class>
struct ClassInstance
{
    RegisteredClass registered_class;

    ClassInstance()
    {
        registered_class = g_global_class_table.Register<Class>();
        AssertThrow(registered_class.IsValid());
    }
};

template <class ClassName>
class Class : public ClassBase
{
    using ClassNameSequence = typename ClassName::Sequence;

public:
    static constexpr const char *GetName() { return ClassNameSequence::Data(); }

    Class()
        : ClassBase()
    {
        // class_fields.Set<decltype(*this)>({ });
    }

    Class(const Class &other) = delete;
    Class &operator=(const Class &other) = delete;

    Class(Class &&other) = delete;
    Class &operator=(Class &&other) noexcept = delete;

    virtual ~Class() = default;

    static const Class &GetInstance()
    {
        static ClassInstance<Class> instance;

        return *static_cast<Class *>(instance.registered_class.GetRefCounted().Get());
    }

    // ClassFields &GetFields()
    // {
    //     return class_fields.At<decltype(*this)>();
    // }
};
} // namespace hyperion

#endif