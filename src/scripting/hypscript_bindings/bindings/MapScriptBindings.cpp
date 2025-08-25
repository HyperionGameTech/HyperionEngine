#ifdef HYP_SCRIPT

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMMap.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

static struct MapScriptBindings : ScriptBindingsBase
{
    MapScriptBindings()
        : ScriptBindingsBase(TypeId::ForType<VMMap>())
    {
    }

    virtual void Generate(scriptapi2::Context& context) override
    {
#if 0
        context.Class<VMMap>("Map", String("<K, V>"))
            .Method("$construct", "function< Map, any >", []() -> VMMap
                    {
                        return VMMap();
                    })
            .Method("operator[]", "function< V, any, K >", CxxFn<vm::Value, VMMap*, vm::Value, [](VMMap* map, vm::Value key) -> vm::Value
                                                               {
                                                                   if (!map)
                                                                   {
                                                                       return vm::Value { vm::Value::HEAP_POINTER, { .internal = { .ptr = nullptr } } };
                                                                   }

                                                                   VMMap::VMMapKey mapKey;
                                                                   mapKey.key = key;
                                                                   mapKey.hash = key.GetHashCode().Value();

                                                                   vm::Value* value = map->GetElement(mapKey);

                                                                   if (!value)
                                                                   {
                                                                       return vm::Value { vm::Value::HEAP_POINTER, { .internal = { .ptr = nullptr } } };
                                                                   }

                                                                   return *value;
                                                               }>)
            .Method("operator[]=", "function< void, any, K, V >", CxxFn<void, VMMap*, vm::Value, vm::Value, [](VMMap* map, vm::Value key, vm::Value value) -> void
                                                                      {
                                                                          if (!map)
                                                                          {
                                                                              return;
                                                                          }

                                                                          VMMap::VMMapKey mapKey;
                                                                          mapKey.key = key;
                                                                          mapKey.hash = key.GetHashCode().Value();

                                                                          map->SetElement(mapKey, value);
                                                                      }>)
            .Method("size", "function< int, any >", CxxFn<int32, VMMap*, [](VMMap* map) -> int32
                                                        {
                                                            if (!map)
                                                            {
                                                                return 0;
                                                            }

                                                            return int32(map->GetSize());
                                                        }>)
            .StaticMethod("from", "function< Map, any, any >", CxxFn<VMMap, const void*, VMArray*, [](const void*, VMArray* array) -> VMMap
                                                                   {
                                                                       // Create VMMap from Array of pairs
                                                                       if (!array)
                                                                       {
                                                                           return VMMap();
                                                                       }

                                                                       VMMap map;

                                                                       // Each element of the array is a pair of key and value (VMArray)
                                                                       for (SizeType index = 0; index < array->GetSize(); index++)
                                                                       {
                                                                           Value& element = array->AtIndex(index);

                                                                           if (element.GetType() != vm::Value::ValueType::HEAP_POINTER)
                                                                           {
                                                                               continue;
                                                                           }

                                                                           if (element.GetValue().internal.ptr == nullptr)
                                                                           {
                                                                               continue;
                                                                           }

                                                                           VMArray* pairArrayPtr = nullptr;
                                                                           VMObject* pairObjectPtr = nullptr;

                                                                           if (!(pairObjectPtr = element.GetValue().internal.ptr->GetPointer<VMObject>()))
                                                                           {
                                                                               continue;
                                                                           }

                                                                           // Get __intern member
                                                                           Member* internMember = pairObjectPtr->LookupMemberFromHash(hashFnv1("__intern"));

                                                                           if (!internMember)
                                                                           {
                                                                               continue;
                                                                           }

                                                                           if (!internMember->value.GetPointer<VMArray>(&pairArrayPtr))
                                                                           {
                                                                               continue;
                                                                           }

                                                                           if (pairArrayPtr->GetSize() < 2)
                                                                           {
                                                                               continue;
                                                                           }

                                                                           Value& key = pairArrayPtr->AtIndex(0);
                                                                           Value& value = pairArrayPtr->AtIndex(1);

                                                                           const HashCode keyHash = key.GetHashCode();

                                                                           VMMap::VMMapKey mapKey;
                                                                           mapKey.key = key;
                                                                           mapKey.hash = keyHash.Value();

                                                                           map.SetElement(mapKey, value);
                                                                       }

                                                                       return map;
                                                                   }>)
            .Build();
#endif
    }

} mapScriptBindings {};

} // namespace bindings
} // namespace script
} // namespace hyperion

#endif