#pragma region ${hyp_class.name}

namespace hyperion {

HYP_BEGIN_CLASS(${hyp_class.name}, ${f"NAME(\"{hyp_class.base_class.name}\")" if hyp_class.base_class else "{}"}, ${','.join([f"HypClassAttribute(\"{name.lower()}\", \"{value}\")" for name, value in hyp_class.attributes])})
    <% s = "" %> \
    % for i in range(0, len(hyp_class.members)):
        \
        <% member = hyp_class.members[i] %> \
        % if member.member_type == HypMemberType.FIELD:
            <% s += f"HypField(NAME(HYP_STR({member.name})), &Type::{member.name}, offsetof(Type, {member.name}))" %> \
        % elif member.member_type == HypMemberType.METHOD:
            <% s += f"HypMethod(NAME(HYP_STR({member.name})), &Type::{member.name})" %> \
        % elif member.member_type == HypMemberType.PROPERTY:
            <% property_args = ', '.join(member.args) %> \
            <% s += f"HypProperty(NAME(HYP_STR({member.name})), {property_args})" %> \
        % endif
        \
        <% s += ',' if i != len(hyp_class.members) - 1 else '' %> \
        \
        <% s += '\n' %>
    % endfor
    ${s}
HYP_END_CLASS

} // namespace hyperion

#pragma endregion ${hyp_class.name}