#ifndef HYPERION_V2_SCRIPT_BINDING_DEF_GENERATED_HPP
#define HYPERION_V2_SCRIPT_BINDING_DEF_GENERATED_HPP

// Include <ScriptApi.hpp> before including this file

#include <type_traits>

namespace hyperion {
#pragma region Member_Functions




    
    

    template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)()>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)());
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)());
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, ReturnType(ThisType::*MemFn)()>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)());
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)());
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)() const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)());
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)());
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, ReturnType(ThisType::*MemFn)() const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)());
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)());
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(Arg0Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(Arg0Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(Arg0Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(Arg0Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type)>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    
    

    template <class ReturnType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = GetArgument<0, ThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, ReturnType(ThisType::*MemFn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type) const>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }




#pragma endregion
#pragma region Functions





    

    template <class ReturnType, ReturnType(*Fn)()>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 0);

        using Normalized = NormalizedType<ReturnType>;

        

        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn());
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn());
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, ReturnType(*Fn)(Arg0Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, ReturnType(*Fn)(Arg0Type, Arg1Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<5, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<5, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<6, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<5, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<6, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<7, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<5, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<6, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<7, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<8, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }


    

    template <class ReturnType, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, class Arg9Type, ReturnType(*Fn)(Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type, Arg5Type, Arg6Type, Arg7Type, Arg8Type, Arg9Type)>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        using Normalized = NormalizedType<ReturnType>;

        

            auto &&arg0 = GetArgument<0, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<1, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<2, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<3, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<4, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<5, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<6, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<7, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<8, Arg8Type>(params);

            
            auto &&arg9 = GetArgument<9, Arg9Type>(params);

            
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }



#pragma endregion
#pragma region Constructors





    

    template <class Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 1);

        

        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type());
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type());
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type());
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type());
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type());
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type());
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type());
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type());
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(), result);

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


    

    template <class Type, class Arg0Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 2);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 3);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 4);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 5);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 6);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 7);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 8);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 9);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 10);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8)), result);

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


    

    template <class Type, class Arg0Type, class Arg1Type, class Arg2Type, class Arg3Type, class Arg4Type, class Arg5Type, class Arg6Type, class Arg7Type, class Arg8Type, class Arg9Type>
    HYP_SCRIPT_FUNCTION(CxxCtor)
    {
        HYP_SCRIPT_CHECK_ARGS(==, 11);

        

            auto &&arg0 = GetArgument<1, Arg0Type>(params);

            
            auto &&arg1 = GetArgument<2, Arg1Type>(params);

            
            auto &&arg2 = GetArgument<3, Arg2Type>(params);

            
            auto &&arg3 = GetArgument<4, Arg3Type>(params);

            
            auto &&arg4 = GetArgument<5, Arg4Type>(params);

            
            auto &&arg5 = GetArgument<6, Arg5Type>(params);

            
            auto &&arg6 = GetArgument<7, Arg6Type>(params);

            
            auto &&arg7 = GetArgument<8, Arg7Type>(params);

            
            auto &&arg8 = GetArgument<9, Arg8Type>(params);

            
            auto &&arg9 = GetArgument<10, Arg9Type>(params);

            
        
        if constexpr (std::is_same_v<void, Type>) {
            HYP_SCRIPT_RETURN_VOID(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<int32_t, Type>) {
            HYP_SCRIPT_RETURN_INT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<int64_t, Type>) {
            HYP_SCRIPT_RETURN_INT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<uint32_t, Type>) {
            HYP_SCRIPT_RETURN_UINT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<uint64_t, Type>) {
            HYP_SCRIPT_RETURN_UINT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<float, Type>) {
            HYP_SCRIPT_RETURN_FLOAT32(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<double, Type>) {
            HYP_SCRIPT_RETURN_FLOAT64(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else if constexpr (std::is_same_v<bool, Type>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)));
        } else {
            HYP_SCRIPT_CREATE_PTR(Type(std::forward<Arg0Type>(arg0), std::forward<Arg1Type>(arg1), std::forward<Arg2Type>(arg2), std::forward<Arg3Type>(arg3), std::forward<Arg4Type>(arg4), std::forward<Arg5Type>(arg5), std::forward<Arg6Type>(arg6), std::forward<Arg7Type>(arg7), std::forward<Arg8Type>(arg8), std::forward<Arg9Type>(arg9)), result);

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



#pragma endregion


} // namespace hyperion

#endif // HYPERION_V2_SCRIPT_BINDING_DEF_GENERATED_HPP