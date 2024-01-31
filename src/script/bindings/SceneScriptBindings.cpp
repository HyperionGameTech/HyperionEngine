#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

#include <scene/Scene.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <Engine.hpp>

namespace hyperion {

using namespace v2;

namespace script {
namespace bindings {

using namespace vm;

struct ComponentInterfacePtrWrapper
{
    ComponentInterfaceBase *ptr = nullptr;
};

struct ComponentPropertyPtrWrapper
{
    ComponentProperty *ptr = nullptr;
};

/*! \brief Type-erased wrapper for a component and its interface.
 *
 *  This is used to allow the script to access the properties of a component
 *  without knowing the type of the component.
 */
struct ComponentWrapper
{
    ComponentInterfaceBase  *interface_ptr = nullptr;
    void                    *component_ptr = nullptr;
};

static struct SceneScriptBindings : ScriptBindingsBase
{
    SceneScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Scene>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        context.Class<Name>("Name")
            .StaticMethod("$invoke", "function< Name, Class, String >", CxxFn< Name, const void *, const String &,
                [](const void *, const String &str) -> Name
                {
                    return CreateNameFromDynamicString(ANSIString(str.Data()));
                }
            >)
            .Build();

        context.Class<ComponentPropertyPtrWrapper>("ComponentProperty")
            .Method("is_read_only", "function< bool, ComponentProperty >", CxxFn< bool, ComponentPropertyPtrWrapper,
                [](ComponentPropertyPtrWrapper wrapper) -> bool
                {
                    if (!wrapper.ptr) {
                        return false;
                    }

                    return wrapper.ptr->IsReadOnly();
                }
            >)
            .Method("is_writable", "function< bool, ComponentProperty >", CxxFn< bool, ComponentPropertyPtrWrapper,
                [](ComponentPropertyPtrWrapper wrapper) -> bool
                {
                    if (!wrapper.ptr) {
                        return false;
                    }

                    return wrapper.ptr->IsWritable();
                }
            >)
            .Build();

        context.Class<ComponentInterfacePtrWrapper>("ComponentInterface")
            .Method("get_property", "function< ComponentProperty, ComponentInterface, Name >", CxxFn<
                ComponentPropertyPtrWrapper, ComponentInterfacePtrWrapper, Name,
                [](ComponentInterfacePtrWrapper wrapper, Name name) -> ComponentPropertyPtrWrapper
                {
                    if (!wrapper.ptr) {
                        return ComponentPropertyPtrWrapper { nullptr };
                    }

                    ComponentProperty *property_ptr = wrapper.ptr->GetProperty(name);

                    return ComponentPropertyPtrWrapper { property_ptr };
                }
            >)
            .Build();

        context.Class<ComponentWrapper>("Component")
            //  Overload methods `operator[]` and `operator[]=`
            .Method("operator[]", "function< any, any, String >", CxxFn<
                ComponentProperty::Value, ComponentWrapper, const String &,
                [](ComponentWrapper wrapper, const String &name) -> ComponentProperty::Value
                {
                    if (!wrapper.interface_ptr || !wrapper.component_ptr) {
                        return { };
                    }

                    const ComponentProperty *property_ptr = wrapper.interface_ptr->GetProperty(CreateNameFromDynamicString(ANSIString(name.Data())));

                    if (!property_ptr) {
                        return { };
                    }

                    auto *getter = property_ptr->GetGetter();

                    if (!getter) {
                        return { };
                    }

                    return getter(wrapper.component_ptr);
                }
            >)

            // @TODO operator[]=

            .Method("get_property", "function< any, any, Name >", CxxFn<
                ComponentProperty::Value, ComponentWrapper, Name,
                [](ComponentWrapper wrapper, Name name) -> ComponentProperty::Value
                {
                    if (!wrapper.interface_ptr || !wrapper.component_ptr) {
                        return { };
                    }

                    const ComponentProperty *property_ptr = wrapper.interface_ptr->GetProperty(name);

                    if (!property_ptr) {
                        return { };
                    }

                    auto *getter = property_ptr->GetGetter();

                    if (!getter) {
                        return { };
                    }

                    return getter(wrapper.component_ptr);
                }
            >)
            .Method("set_property", "function< void, any, Name, any >", CxxFn<
                void, ComponentWrapper, Name, vm::Value,
                [](ComponentWrapper wrapper, Name name, vm::Value value) -> void
                {
                    if (!wrapper.interface_ptr || !wrapper.component_ptr) {
                        return;
                    }

                    const ComponentProperty *property_ptr = wrapper.interface_ptr->GetProperty(name);

                    if (!property_ptr) {
                        return;
                    }

                    auto *setter = property_ptr->GetSetter();

                    if (!setter) {
                        return;
                    }

                    switch (value.GetType()) {
                    case vm::Value::I8:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().i8));
                        break;
                    case vm::Value::I16:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().i16));
                        break;
                    case vm::Value::I32:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().i32));
                        break;
                    case vm::Value::I64:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().i64));
                        break;
                    case vm::Value::U8:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().u8));
                        break;
                    case vm::Value::U16:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().u16));
                        break;
                    case vm::Value::U32:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().u32));
                        break;
                    case vm::Value::U64:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().u64));
                        break;
                    case vm::Value::F32:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().f));
                        break;
                    case vm::Value::F64:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().d));
                        break;
                    case vm::Value::BOOLEAN:
                        setter(wrapper.component_ptr, ComponentProperty::Value(value.GetValue().b));
                        break;
                    // now handle pointer types
                    case vm::Value::HEAP_POINTER: {
                        union {
                            VMString *string_ptr;
                            VMObject *object_ptr;
                        };

                        if (value.GetPointer<VMString>(&string_ptr)) {
                            setter(wrapper.component_ptr, ComponentProperty::Value(string_ptr->GetString()));
                        } else if (value.GetPointer<VMObject>(&object_ptr)) {

                            // Look for __intern property
                            vm::Member *intern_member = object_ptr->LookupMemberFromHash(hash_fnv_1("__intern"), false);

                            if (intern_member) {
                                // __intern property found, try to get Vec3f, etc. from it

                                union {
                                    Vec3f       *vec3f_ptr;
                                    Vec3i       *vec3i_ptr;
                                    Vec3u       *vec3u_ptr;
                                    Vec4f       *vec4f_ptr;
                                    Vec4i       *vec4i_ptr;
                                    Vec4u       *vec4u_ptr;
                                    Quaternion  *quat_ptr;
                                    Matrix4     *mat4_ptr;
                                };

                                if (intern_member->value.GetPointer<Vec3f>(&vec3f_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec3f_ptr));
                                } else if (intern_member->value.GetPointer<Vec3i>(&vec3i_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec3i_ptr));
                                } else if (intern_member->value.GetPointer<Vec3u>(&vec3u_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec3u_ptr));
                                } else if (intern_member->value.GetPointer<Vec4f>(&vec4f_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec4f_ptr));
                                } else if (intern_member->value.GetPointer<Vec4i>(&vec4i_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec4i_ptr));
                                } else if (intern_member->value.GetPointer<Vec4u>(&vec4u_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*vec4u_ptr));
                                } else if (intern_member->value.GetPointer<Quaternion>(&quat_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*quat_ptr));
                                } else if (intern_member->value.GetPointer<Matrix4>(&mat4_ptr)) {
                                    setter(wrapper.component_ptr, ComponentProperty::Value(*mat4_ptr));
                                } else {
                                    DebugLog(LogType::Warn, "Unhandled __intern pointer type!\n");
                                }
                            } else {
                                // __intern property not found, we can't do anything
                                DebugLog(LogType::Warn, "Object does not have __intern property\n");
                            }

                        } else {
                            DebugLog(LogType::Warn, "Unhandled pointer type\n");
                        }

                        break;
                    }
                    }
                }
            >)
            .Build();

        context.Class<Handle<Scene>>("Scene")
            .Method("$construct", "function< Scene, any >", CxxFn< Handle<Scene>, const void *, ScriptCreateObject<Scene> >)
            .Method("get_id", "function< uint, Scene >", CxxFn< UInt32, const Handle<Scene> &, ScriptGetHandleIDValue<Scene> >)
            .Build();

        context.Class<TransformComponent>("TransformComponent")
            .Build();

        // global for now
        context.Global("get_component", "function< any, any, uint, uint >", CxxFn<
            ComponentWrapper, const Handle<Scene> &, UInt32, UInt32,
            [](const Handle<Scene> &scene, UInt32 native_type_id /* temp */, UInt32 entity_id) -> ComponentWrapper
            {
                if (!scene.IsValid()) {
                    return ComponentWrapper { nullptr, nullptr };
                }

                void *component_ptr = scene->GetEntityManager()->TryGetComponent(TypeID(native_type_id), entity_id);

                if (!component_ptr) {
                    return ComponentWrapper { nullptr, nullptr };
                }

                return ComponentWrapper {
                    GetComponentInterface(TypeID(native_type_id)),
                    component_ptr
                };
            }
        >);

    }

} scene_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion