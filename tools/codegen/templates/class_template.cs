public static class ${hyp_class.name}Extensions
{
% for i in range(0, len(hyp_class.members)):
    <% return_type = hyp_class.members[i].return_type %> \
    <% return_type_name = return_type[0] %> \
    % if hyp_class.members[i].member_type == HypMemberType.METHOD:
    public static ${return_type_name} ${hyp_class.members[i].name}(this ${hyp_class.name} obj${", " + ", ".join([f"{x[0]} {x[1]}" for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
    {
        % if return_type_name == "void":
            % if hyp_class.class_type == HypClassType.STRUCT:
        HypObject.GetMethod(HypClass.GetClass<${hyp_class.name}>(), new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""});
            % else:
        obj.GetMethod(new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""});
            %endif
        % else:
            % if hyp_class.class_type == HypClassType.STRUCT:
        return (${return_type_name})HypObject.GetMethod(HypClass.GetClass<${hyp_class.name}>(), new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
            .GetValue();
            % else:
        return (${return_type_name})obj.GetMethod(new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
            .GetValue();
            %endif
        % endif
    }
    % elif hyp_class.members[i].member_type == HypMemberType.FIELD and return_type_name == "ScriptableDelegate":
    public static Func<${", ".join(return_type[2])}> Get${hyp_class.members[i].name}Delegate(this ${hyp_class.name} obj)
    {
    }
    % endif
% endfor
}