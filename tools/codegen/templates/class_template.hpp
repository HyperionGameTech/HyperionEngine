<% is_component = hyp_class.has_attribute("component") %> \
<% has_struct_size = hyp_class.is_class and hyp_class.has_attribute("size") %> \
<% has_scriptable_methods = hyp_class.is_class and hyp_class.has_scriptable_methods %> \

% if is_component:
${"#include <scene/ecs/ComponentInterface.hpp>"}
% endif

% if has_scriptable_methods:
${"#include <dotnet/Object.hpp>"}
${"#include <dotnet/Class.hpp>"}
${"#include <dotnet/Method.hpp>"}
% endif

namespace hyperion {

#pragma region ${hyp_class.name} Reflection Data

<% start_macro_names = { HypClassType.CLASS: 'HYP_BEGIN_CLASS', HypClassType.STRUCT: 'HYP_BEGIN_STRUCT', HypClassType.ENUM: 'HYP_BEGIN_ENUM' } %> \
<% end_macro_names = { HypClassType.CLASS: 'HYP_END_CLASS', HypClassType.STRUCT: 'HYP_END_STRUCT', HypClassType.ENUM: 'HYP_END_ENUM' } %> \

<% class_attributes = ','.join([f"HypClassAttribute(\"{name.lower()}\", {format_attribute_value(value, attr_type)})" for name, value, attr_type in hyp_class.attributes]) %> \
\
${start_macro_names[hyp_class.class_type]}(${hyp_class.name}${(f", NAME(\"{hyp_class.base_class.name}\")" if hyp_class.base_class else ", {}") if hyp_class.is_class else ''}${(', ' + class_attributes) if class_attributes else ''}) \
    <% s = "" %> \
    % for i in range(0, len(hyp_class.members)):
        <% member = hyp_class.members[i] %> \
        <% attributes = ("Span<const HypClassAttribute> { { " + ', '.join([f"HypClassAttribute(\"{name.lower()}\", {format_attribute_value(value, attr_type)})" for name, value, attr_type in member.attributes]) + " } }") if len(member.attributes) > 0 else '' %> \
        % if member.member_type == HypMemberType.FIELD:
            <% s += f"HypField(NAME(HYP_STR({member.name})), &Type::{member.name}, offsetof(Type, {member.name}){', ' + attributes if len(member.attributes) > 0 else ''})" %> \
        % elif member.member_type == HypMemberType.METHOD:
            <% s += f"HypMethod(NAME(HYP_STR({member.name})), &Type::{member.name}{', ' + attributes if len(member.attributes) > 0 else ''})" %> \
        % elif member.member_type == HypMemberType.PROPERTY:
            <% property_args = ', '.join(member.args) %> \
            <% s += f"HypProperty(NAME(HYP_STR({member.name})), {property_args})" %> \
        % elif member.member_type == HypMemberType.ENUMERATOR:
            <% s += f"HypProperty(NAME(HYP_STR({to_pascal_case(member.name)})), HypPropertyGetter((Type(*)(void))([](void) {{ return Type::{member.name}; }})))" %> \
        % endif
        <% s += ',' if i != len(hyp_class.members) - 1 else '' %> \
        <% s += '\n' %> \
    % endfor
    ${s}
${end_macro_names[hyp_class.class_type]}

#pragma endregion ${hyp_class.name} Reflection Data

% if has_scriptable_methods:
#pragma region ${hyp_class.name} Scriptable Methods
    % for member in hyp_class.members:
        % if member.is_method and member.has_attribute("Scriptable"):
            <% method_args_string_sig = ', '.join([f"{arg_type} {arg_name}" for arg_type, arg_name in member.args]) if len(member.args) > 0 else "" %> \
            <% method_args_string_call = ', '.join([f"{arg_name}" for arg_type, arg_name in member.args]) if len(member.args) > 0 else "" %> \

            % if member.method_return_type == "void":
${f"void {hyp_class.name}::{member.name}({method_args_string_sig})" + (" const" if member.is_const_method else "")}
${"{"}
${"    if (dotnet::Object *managed_object = GetManagedObject()) {"}
${f"        if (dotnet::Method *method_ptr = managed_object->GetClass()->GetMethod(\"{member.name}\")) {{"}
${f"            managed_object->InvokeMethod<void>(method_ptr{(', ' + method_args_string_call) if len(method_args_string_call) > 0 else ''});"}
${"            return;"}
${"        }"}
${"    }"}
${""}
${f"    {member.name}_Impl({method_args_string_call});"}
${"}"}
            % else:
${f"{member.method_return_type} {hyp_class.name}::{member.name}({method_args_string_sig})" + (" const" if member.is_const_method else "")}
${"{"}
${"    if (dotnet::Object *managed_object = GetManagedObject()) {"}
${f"        if (dotnet::Method *method_ptr = managed_object->GetClass()->GetMethod(\"{member.name}\")) {{"}
${f"            return managed_object->InvokeMethod<{member.method_return_type}>(method_ptr{(', ' + method_args_string_call) if len(method_args_string_call) > 0 else ''});"}
${"        }"}
${"    }"}
${""}
${f"    return {member.name}_Impl({method_args_string_call});"}
${"}"}
            % endif
        % endif
    % endfor

#pragma endregion ${hyp_class.name} Scriptable Methods
% endif

% if is_component:
    ${f"HYP_REGISTER_COMPONENT({hyp_class.name});"}
% endif

% if has_struct_size:
    ${f"static_assert(sizeof({hyp_class.name}) == {hyp_class.get_attribute('size')}, \"Expected sizeof({hyp_class.name}) to be {hyp_class.get_attribute('size')} bytes\");"}
% endif

} // namespace hyperion

