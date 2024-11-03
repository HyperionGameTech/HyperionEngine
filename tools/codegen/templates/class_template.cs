public static class ${hyp_class.name}Extensions
{
% for i in range(0, len(hyp_class.members)):
    % if hyp_class.members[i].member_type == HypMemberType.METHOD:
    public static ${hyp_class.members[i].return_type} ${hyp_class.members[i].name}(this ${hyp_class.name} obj${", " + ", ".join([f"{x[0]} {x[1]}" for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
    {
        % if hyp_class.members[i].return_type == "void":
            % if hyp_class.class_type == HypClassType.STRUCT:
        HypObject.GetMethod(HypClass.GetClass<${hyp_class.name}>(), new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""});
            % else:
        obj.GetMethod(new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""});
            %endif
        % else:
            % if hyp_class.class_type == HypClassType.STRUCT:
        return (${hyp_class.members[i].return_type})HypObject.GetMethod(HypClass.GetClass<${hyp_class.name}>(), new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
            .GetValue();
            % else:
        return (${hyp_class.members[i].return_type})obj.GetMethod(new Name("${hyp_class.members[i].name}", weak: true))
            .Invoke(obj${", " + ", ".join([x[1] for x in hyp_class.members[i].args]) if len(hyp_class.members[i].args) > 0 else ""})
            .GetValue();
            %endif
        % endif
    }
    % endif
% endfor
}