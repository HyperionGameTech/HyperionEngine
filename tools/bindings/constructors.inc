<%def name="do_ctor(num_args)">
    <% arg_template_str = ', '.join(["class Arg{}Type".format(i) for i in range(0, num_args)]) %>

    template <class Type${", {}".format(arg_template_str) if len(arg_template_str) else ""}>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args + 1});

        <% invoke_args = [] %>

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i + 1}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(${', '.join(invoke_args)}));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(${', '.join(invoke_args)}), result);

            const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Type>();
            AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

            const auto prototype_it = params.api_instance.class_bindings.class_prototypes.Find(class_name_it->second);
            AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.End(), "Class not registered!");

            vm::VMObject result_value(prototype_it->second); // construct from prototype
            HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

            HYP_SCRIPT_CREATE_PTR(result_value, ptr);

            HYP_SCRIPT_RETURN(ptr);
        }
    }
</%def>

${do_ctor(0)}
${do_ctor(1)}
${do_ctor(2)}
${do_ctor(3)}
${do_ctor(4)}
${do_ctor(5)}
${do_ctor(6)}
${do_ctor(7)}
${do_ctor(8)}
${do_ctor(9)}
${do_ctor(10)}