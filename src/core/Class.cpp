#include "Class.hpp"

namespace hyperion {

TypeMap<ClassFields> ClassInitializerBase::class_fields = {};

GlobalClassTable g_global_class_table = { };

// RegisteredClass GlobalClassTable::Register(ClassBase *class_object)
// {
//     std::lock_guard guard(mtx);

//     AssertThrowMsg(index < max_class_objects, "Too many class objects registered");

//     UInt object_index = index++;
//     class_objects[object_index] = std::move(class_object);

//     return { object_index };
// }

const RC<ClassBase> &RegisteredClass::GetRefCounted() const
{
    static const RC<ClassBase> null_class = nullptr;

    if (!IsValid()) {
        return null_class;
    }

    return g_global_class_table.class_objects[index];
}

// ClassBase &RegisteredClass::operator*()
// {
//     AssertThrow(IsValid());

//     return *g_global_class_table.class_objects[index];
// }

// const ClassBase &RegisteredClass::operator*() const
// {
//     AssertThrow(IsValid());

//     return *g_global_class_table.class_objects[index];
// }

// ClassBase *RegisteredClass::operator->()
// {
//     AssertThrow(IsValid());

//     return g_global_class_table.class_objects[index].Get();
// }

// const ClassBase *RegisteredClass::operator->() const
// {
//     AssertThrow(IsValid());

//     return g_global_class_table.class_objects[index].Get();
// }

} // namespace hyperion