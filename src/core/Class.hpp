#ifndef HYPERION_V2_CORE_CLASS_HPP
#define HYPERION_V2_CORE_CLASS_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeMap.hpp>

#include <script/ScriptApi.hpp>
#include <script/vm/Value.hpp>

#include <type_traits>

namespace hyperion::v2 {

using ClassField = API::NativeMemberDefine;
using ClassFields = Array<ClassField>; //FlatMap<ANSIString, FieldInfo>;
// global variable of all class fields
// this must strictly be used on one thread

struct ClassInitializerBase
{
    static TypeMap<ClassFields> class_fields;
};


template <class Class>
struct ClassInitializer : ClassInitializerBase
{
    ClassInitializer(std::add_pointer_t<ClassFields(void)> &&fn)
    {
        ClassInitializerBase::class_fields.Set<Class>(fn());
    }
};

template <class ClassName>
struct Class
{
    using ClassNameSequence = typename ClassName::Sequence;

public:
    static constexpr const char *GetName() { return ClassNameSequence::Data(); }

    Class()
    {
        // class_fields.Set<decltype(*this)>({ });
    }

    Class(const Class &other) = delete;
    Class &operator=(const Class &other) = delete;

    Class(Class &&other) = delete;
    Class &operator=(Class &&other) noexcept = delete;

    ~Class()
    {
        // class_fields.Remove<decltype(*this)>();
    }

    static Class &GetInstance()
    {
        static Class instance;
        return instance;
    }

    // ClassFields &GetFields()
    // {
    //     return class_fields.At<decltype(*this)>();
    // }
};
} // namespace hyperion::v2

#endif