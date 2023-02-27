<%def name="do_member_fn(num_args, is_const)">
    <% arg_template_str = ', '.join(["class Arg{}Type".format(i) for i in range(0, num_args)]) %>

    template <class ReturnType, class ThisType${", {}".format(arg_template_str) if len(arg_template_str) else ""}, ReturnType(ThisType::*MemFn)(${', '.join(["Arg{}Type".format(i) for i in range(0, num_args)])})${" const" if is_const else ""}>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args + 1});

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        <% invoke_args = [] %>

        auto &&this_arg = GetArgument<0, ThisType *>(params);

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i + 1}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int32_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT32((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int64_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT64((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint32_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT32((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint64_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT64((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<float, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT32((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<double, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT64((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<bool, Normalized>) {
            HYP_SCRIPT_RETURN_BOOLEAN((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else {
            HYP_SCRIPT_CREATE_PTR((this_arg->*MemFn)(${', '.join(invoke_args)}), result);

            const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Normalized>();
            AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

            const auto prototype_it = params.api_instance.class_bindings.class_prototypes.find(class_name_it->second);
            AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.end(), "Class not registered!");

            vm::VMObject result_value(prototype_it->second); // construct from prototype
            HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

            HYP_SCRIPT_CREATE_PTR(result_value, ptr);

            HYP_SCRIPT_RETURN(ptr);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType${", {}".format(arg_template_str) if len(arg_template_str) else ""}, ReturnType(ThisType::*MemFn)(${', '.join(["Arg{}Type".format(i) for i in range(0, num_args)])})${" const" if is_const else ""}>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args + 1});

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        <% invoke_args = [] %>

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i + 1}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int32_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT32(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int64_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT64(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint32_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT32(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint64_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT64(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<float, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT32(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<double, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT64(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<bool, Normalized>) {
            HYP_SCRIPT_RETURN_BOOLEAN(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else {
            HYP_SCRIPT_CREATE_PTR(((*this_arg).*MemFn)(${', '.join(invoke_args)}), result);

            const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Normalized>();
            AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

            const auto prototype_it = params.api_instance.class_bindings.class_prototypes.find(class_name_it->second);
            AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.end(), "Class not registered!");

            vm::VMObject result_value(prototype_it->second); // construct from prototype
            HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

            HYP_SCRIPT_CREATE_PTR(result_value, ptr);

            HYP_SCRIPT_RETURN(ptr);
        }
    }
</%def>

% for i in range(0, 10):
    ${do_member_fn(i, False)}
    ${do_member_fn(i, True)}
% endfor