/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

bool HypStruct::CreateStructInstance(dotnet::ObjectReference &out_object_reference, const void *object_ptr, SizeType size) const
{
    struct ManagedStructInitializerContext
    {
        const void  *ptr;
        SizeType    size;
    };

    AssertThrow(object_ptr != nullptr);

    if (dotnet::Class *managed_class = HypClass::GetManagedClass()) {
        ManagedStructInitializerContext context;
        context.ptr = object_ptr;
        context.size = size;

        out_object_reference = managed_class->NewManagedObject(&context, [](void *context_ptr, void *object_ptr, uint32 object_size)
        {
            ManagedStructInitializerContext &context = *static_cast<ManagedStructInitializerContext *>(context_ptr);

            AssertThrowMsg(object_size == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %llu but got %u",
                context.size, object_size);

            Memory::MemCpy(object_ptr, context.ptr, context.size);
        });

        return true;
    }

    return false;
}

} // namespace hyperion