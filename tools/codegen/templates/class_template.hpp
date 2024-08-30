#pragma region ${hyp_class.name}

HYP_BEGIN_CLASS(${hyp_class.name}, ${f"NAME(\"{hyp_class.base_class.name}\")" if hyp_class.base_class else "{}"}, ${','.join([f"HypClassAttribute(\"{name.lower()}\", \"{value}\")" for name, value in hyp_class.attributes])})
    % for i in range(0, len(hyp_class.members)):
        <% s = "" %> \
        \
        <% member = hyp_class.members[i] %> \
        % if member.member_type == HypMemberType.FIELD:
            <% s += f"HypField(NAME(HYP_STR({member.name})), &Type::{member.name}, offsetof(Type, {member.name}))" %> \
        % elif member.member_type == HypMemberType.METHOD:
            <% s += f"HypMethod(NAME(HYP_STR({member.name})), &Type::{member.name})" %> \
        % endif
        \
        <% s += ',' if i != len(hyp_class.members) - 1 else '' %> \
        \
        ${s}
    % endfor
HYP_END_CLASS

#pragma endregion ${hyp_class.name}