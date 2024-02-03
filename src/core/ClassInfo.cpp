#include <core/ClassInfo.hpp>

namespace hyperion {

GlobalClassInfoTable g_global_class_info_table = { };

// RegisteredClass GlobalClassInfoTable::Register(ClassInfoBase *class_object)
// {
//     std::lock_guard guard(mtx);

//     AssertThrowMsg(index < max_class_objects, "Too many class objects registered");

//     UInt object_index = index++;
//     class_objects[object_index] = std::move(class_object);

//     return { object_index };
// }

const RC<ClassInfoBase> &RegisteredClass::GetRefCounted() const
{
    static const RC<ClassInfoBase> null_class = nullptr;

    if (!IsValid()) {
        return null_class;
    }

    return g_global_class_info_table.class_objects[index];
}

// ClassInfoBase &RegisteredClass::operator*()
// {
//     AssertThrow(IsValid());

//     return *g_global_class_table.class_objects[index];
// }

// const ClassInfoBase &RegisteredClass::operator*() const
// {
//     AssertThrow(IsValid());

//     return *g_global_class_table.class_objects[index];
// }

// ClassInfoBase *RegisteredClass::operator->()
// {
//     AssertThrow(IsValid());

//     return g_global_class_table.class_objects[index].Get();
// }

// const ClassInfoBase *RegisteredClass::operator->() const
// {
//     AssertThrow(IsValid());

//     return g_global_class_table.class_objects[index].Get();
// }

} // namespace hyperion