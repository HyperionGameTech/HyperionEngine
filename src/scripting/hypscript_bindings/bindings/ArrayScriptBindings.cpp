#ifdef HYP_SCRIPT

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

static struct ArrayScriptBindings : ScriptBindingsBase
{
    ArrayScriptBindings()
        : ScriptBindingsBase(TypeId::ForType<VMArray>())
    {
    }

    virtual void Generate(scriptapi2::Context& context) override
    {
#if 0
        context.Class<VMArray>("Array", String("<T>"))
            .Method("$construct", "function< Array, any >", CxxCtor<VMArray>)
            .Method("length", "function< int, any >", CxxFn<int32, VMArray*, [](VMArray* array) -> int32
                                                          {
                                                              if (!array)
                                                              {
                                                                  return 0;
                                                              }

                                                              return int32(array->GetSize());
                                                          }>)
            .Method("operator[]", "function< T, any, int >", CxxFn<vm::Value, VMArray*, int32, [](VMArray* array, int32 index) -> vm::Value
                                                                 {
                                                                     if (!array)
                                                                     {
                                                                         return vm::Value { vm::Value::NONE, {} };
                                                                     }

                                                                     if (index < 0 || index >= array->GetSize())
                                                                     {
                                                                         return vm::Value { vm::Value::NONE, {} };
                                                                     }

                                                                     return vm::Value(array->AtIndex(index));
                                                                 }>)
            .Method("operator[]=", "function< void, any, int, T >", CxxFn<void, VMArray*, int32, vm::Value, [](VMArray* array, int32 index, vm::Value value) -> void
                                                                        {
                                                                            if (!array)
                                                                            {
                                                                                return;
                                                                            }

                                                                            if (index < 0 || index >= array->GetSize())
                                                                            {
                                                                                return;
                                                                            }

                                                                            array->AtIndex(index) = value;
                                                                            array->AtIndex(index).Mark();
                                                                        }>)
            .Method("push", "function< void, any, T >", CxxFn<void, VMArray*, vm::Value, [](VMArray* array, Value value) -> void
                                                            {
                                                                if (!array)
                                                                {
                                                                    return;
                                                                }

                                                                array->Push(value);
                                                            }>)
            .Method("pop", "function< void, any >", CxxFn<void, VMArray*, [](VMArray* array) -> void
                                                        {
                                                            if (!array)
                                                            {
                                                                return;
                                                            }

                                                            // @TODO Throw exception if array is empty

                                                            array->Pop();
                                                        }>)
            .StaticMethod("from", "function< Array, any, any >", CxxFn<VMArray, const void*, vm::Value, [](const void*, vm::Value value) -> VMArray
                                                                     {
                                                                         if (value.GetType() != vm::Value::ValueType::HEAP_POINTER)
                                                                         {
                                                                             return VMArray();
                                                                         }

                                                                         if (value.GetValue().internal.ptr == nullptr)
                                                                         {
                                                                             return VMArray();
                                                                         }

                                                                         if (value.GetValue().internal.ptr->GetPointer<VMArray>() == nullptr)
                                                                         {
                                                                             return VMArray();
                                                                         }

                                                                         return VMArray(*value.GetValue().internal.ptr->GetPointer<VMArray>());
                                                                     }>)
            .Build();
#endif
    }

} arrayScriptBindings {};

} // namespace bindings
} // namespace script
} // namespace hyperion

#endif