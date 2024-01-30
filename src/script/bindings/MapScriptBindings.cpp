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
        : ScriptBindingsBase(TypeID::ForType<VMMap>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        context.Class<VMMap>("Map", String("<K, V>"))
            .Method("$construct", "function< Map, any >", CxxCtor<VMMap>)
            .Method("operator[]", "function< V, any, K >", CxxFn<vm::Value, VMMap *, vm::Value,
                [](VMMap *map, vm::Value key) -> vm::Value
                {
                    if (!map) {
                        return vm::Value { vm::Value::HEAP_POINTER, { .ptr = nullptr } };
                    }

                    VMMap::VMMapKey map_key;
                    map_key.key = key;
                    map_key.hash = key.GetHashCode().Value();

                    vm::Value *value = map->GetElement(map_key);

                    if (!value) {
                        return vm::Value { vm::Value::HEAP_POINTER, { .ptr = nullptr } };
                    }

                    return *value;
                }
            >)
            .Method("operator[]=", "function< void, any, K, V >", CxxFn<void, VMMap *, vm::Value, vm::Value,
                [](VMMap *map, vm::Value key, vm::Value value) -> void
                {
                    if (!map) {
                        return;
                    }

                    VMMap::VMMapKey map_key;
                    map_key.key = key;
                    map_key.hash = key.GetHashCode().Value();

                    map->SetElement(map_key, value);
                }
            >)
            .Method("size", "function< int, any >", CxxFn<Int32, VMMap *,
                [](VMMap *map) -> Int32
                {
                    if (!map) {
                        return 0;
                    }

                    return Int32(map->GetSize());
                }
            >)
            .StaticMethod("from", "function< Map, any, any >", CxxFn<VMMap, const void *, VMArray *,
                [](const void *, VMArray *array) -> VMMap
                {
                    // Create VMMap from Array of pairs
                    if (!array) {
                        return VMMap();
                    }

                    VMMap map;

                    // Each element of the array is a pair of key and value (VMArray)
                    for (SizeType index = 0; index < array->GetSize(); index++) {
                        Value &element = array->AtIndex(index);

                        if (element.GetType() != vm::Value::ValueType::HEAP_POINTER) {
                            continue;
                        }

                        if (element.GetValue().ptr == nullptr) {
                            continue;
                        }

                        VMArray *pair_array_ptr = nullptr;
                        VMObject *pair_object_ptr = nullptr;

                        if (!(pair_object_ptr = element.GetValue().ptr->GetPointer<VMObject>())) {
                            continue;
                        }

                        // Get __intern member
                        Member *intern_member = pair_object_ptr->LookupMemberFromHash(hash_fnv_1("__intern"));

                        if (!intern_member) {
                            continue;
                        }

                        if (!intern_member->value.GetPointer<VMArray>(&pair_array_ptr)) {
                            continue;
                        }

                        if (pair_array_ptr->GetSize() < 2) {
                            continue;
                        }

                        Value &key = pair_array_ptr->AtIndex(0);
                        Value &value = pair_array_ptr->AtIndex(1);

                        const HashCode key_hash = key.GetHashCode();

                        VMMap::VMMapKey map_key;
                        map_key.key = key;
                        map_key.hash = key_hash.Value();

                        map.SetElement(map_key, value);
                    }

                    return map;
                }
            >)
            .Build();
    }

} map_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion