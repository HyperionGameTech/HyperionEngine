public enum ${hyp_enum.name}
{
% for i in range(0, len(hyp_enum.values)):
    ${hyp_enum.values[i][0]}${" = " + str(hyp_enum.values[i][1]) if hyp_enum.values[i][1] is not None else ""}${"," if i < len(hyp_enum.values) - 1 else ""}
% endfor
}
