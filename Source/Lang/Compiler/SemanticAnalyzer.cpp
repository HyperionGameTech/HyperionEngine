#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Ast/AstModuleDeclaration.hpp>
#include <Lang/Compiler/Ast/AstUndefined.hpp>
#include <Lang/Compiler/Module.hpp>

#include <Lang/Compiler/TypeSystem/SymbolType.hpp>
#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Containers/Stack.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

SCRIPT_API HYP_DECLARE_LOG_CHANNEL(HypScript);

static inline bool ShouldSkipNestedResolution(const SymbolType& type)
{
    return type.IsObject()
        || type.TypeEqual(*BuiltinTypes::s_stringType)
        || type.HasBase(*BuiltinTypes::s_arrayBaseType)
        || type.HasBase(*BuiltinTypes::s_mapBaseType);
}

const SymbolType* SemanticAnalyzer::Helpers::ResolvePlaceholderType(
    AstVisitor* visitor,
    Module* mod,
    const SymbolType* inputType,
    const SourceLocation& location)
{
    thread_local Stack<Pair<SymbolType*, SymbolType*>> t_resolutionStack;

    if (!inputType)
    {
        return nullptr;
    }

    const SymbolType* originalInputType = inputType;

    inputType = inputType->GetUnaliased();

    for (const Pair<SymbolType*, SymbolType*>& entry : t_resolutionStack)
    {
        if (entry.first == inputType && entry.second != nullptr)
        {
            // HYP_LOG(HypScript, Verbose, "Using cached placeholder type for '{}' : {}", inputType->GetName(), entry.second->ToString());

            return entry.second;
        }
    }

    t_resolutionStack.Push({ const_cast<SymbolType*>(inputType), const_cast<SymbolType*>(inputType) });

    HYP_DEFER({
        AssertDebug(!t_resolutionStack.Empty() && t_resolutionStack.Top().first == inputType);

        t_resolutionStack.Pop();
    });

    if (!inputType->IsPlaceholderType())
    {
        // Check if type has members, static members, or base types that need resolution
        if (inputType->GetMembers().Any() || inputType->GetStaticMembers().Any() || inputType->IsGenericInstanceType())
        {
            bool wasMutated = false;

            Array<SymbolTypeMember> newMembers;
            Array<SymbolTypeMember> newStaticMembers;
            GenericInstanceTypeInfo newGenericInstanceInfo;

            for (size_t memberIndex = 0; memberIndex < inputType->GetMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = inputType->GetMembers()[memberIndex];
                Assert(srcMember.GetType() != nullptr);

                if (ShouldSkipNestedResolution(*srcMember.GetType()))
                {
                    continue;
                }

                const SymbolType* resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.GetType(), location);
                Assert(resolvedMemberType != nullptr);

                if (srcMember.GetType() != resolvedMemberType)
                {
                    if (!wasMutated)
                    {
                        newMembers = inputType->GetMembers();
                        newStaticMembers = inputType->GetStaticMembers();
                        wasMutated = true;
                    }

                    newMembers[memberIndex] = { srcMember.GetName(), const_cast<SymbolType*>(resolvedMemberType), srcMember.GetExpr(), srcMember.IsConst() };
                }
            }

            for (size_t memberIndex = 0; memberIndex < inputType->GetStaticMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = inputType->GetStaticMembers()[memberIndex];
                Assert(srcMember.GetType() != nullptr);

                if (ShouldSkipNestedResolution(*srcMember.GetType()))
                {
                    continue;
                }

                const SymbolType* resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.GetType(), location);
                Assert(resolvedMemberType != nullptr);

                if (srcMember.GetType() != resolvedMemberType)
                {
                    if (!wasMutated)
                    {
                        newMembers = inputType->GetMembers();
                        newStaticMembers = inputType->GetStaticMembers();
                        wasMutated = true;
                    }

                    newStaticMembers[memberIndex] = { srcMember.GetName(), const_cast<SymbolType*>(resolvedMemberType), srcMember.GetExpr(), srcMember.IsConst() };
                }
            }

            if (inputType->IsGenericInstanceType())
            {
                bool genericArgsMutated = false;

                newGenericInstanceInfo = inputType->GetGenericInstanceInfo();

                for (size_t i = 0; i < newGenericInstanceInfo.m_genericArgs.Size(); i++)
                {
                    const SymbolType*& argType = newGenericInstanceInfo.m_genericArgs[i].m_type;
                    Assert(argType != nullptr);

                    if (ShouldSkipNestedResolution(*argType))
                    {
                        continue;
                    }

                    const SymbolType* resolvedArgType = ResolvePlaceholderType(visitor, mod, argType, location);
                    Assert(resolvedArgType != nullptr);

                    if (argType != resolvedArgType)
                    {
                        argType = resolvedArgType;

                        genericArgsMutated = true;
                    }
                }

                if (genericArgsMutated)
                {
                    if (!wasMutated)
                    {
                        newMembers = inputType->GetMembers();
                        newStaticMembers = inputType->GetStaticMembers();
                        wasMutated = true;
                    }
                }
            }

            if (wasMutated)
            {
                SymbolType* newType = inputType->Clone();
                newType->SetMembers(std::move(newMembers));
                newType->SetStaticMembers(std::move(newStaticMembers));

                if (inputType->IsGenericInstanceType())
                {
                    newType->GetGenericInstanceInfo() = std::move(newGenericInstanceInfo);
                }

                return newType;
            }
        }

        return inputType;
    }

    // Try to resolve placeholder type by looking it up in the module
    const SymbolType* resolvedType = mod->LookupSymbolType(inputType->GetName());

    if (resolvedType)
    {
        return resolvedType;
    }

    // Could not resolve placeholder type
    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_placeholder_resolution_failed,
        location,
        originalInputType->GetName()));

    return BuiltinTypes::s_errorType;
}

void SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
    AstVisitor* visitor,
    Module* mod,
    const SourceLocation& location,
    const SymbolType* argType,
    const SymbolType* paramType)
{
    Assert(argType != nullptr);
    Assert(paramType != nullptr);

    if (argType->TypeEqual(*BuiltinTypes::s_errorType))
    {
        return;
    }

    const SymbolType* resolvedParamType = ResolvePlaceholderType(visitor, mod, paramType, location);
    Assert(resolvedParamType != nullptr);

    const SymbolType* resolvedArgType = ResolvePlaceholderType(visitor, mod, argType, location);
    Assert(resolvedArgType != nullptr);

    //// temp
    // SymbolType* resolvedParamType = paramType.Get();
    // SymbolType* resolvedArgType = argType.Get();

    SymbolTypeIncompatibilities incompatibilities;

    if (resolvedParamType->TypeCompatible(
            *resolvedArgType,
            /* strictNumbers */ true,
            /* strictAny */ true,
            /* strictEnum */ true,
            /* strictNull */ true,
            /* strictDownCasting */ true,
            &incompatibilities))
    {
        return;
    }

    if (incompatibilities.Any())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_arg_type_incompatible_more_info,
            location,
            resolvedArgType->ToString(),
            resolvedParamType->ToString(),
            (incompatibilities.Size() > 1
                 ? "\n\t* " + String::Join(MapToArray(incompatibilities, &SymbolTypeIncompatibility::details), "\n\t* ")
                 : " " + incompatibilities[0].details)));
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_arg_type_incompatible,
            location,
            resolvedArgType->ToString(),
            resolvedParamType->ToString()));
    }
}

size_t SemanticAnalyzer::Helpers::FindFreeSlot(
    size_t currentIndex,
    const FlatSet<size_t>& usedIndices,
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    bool isVariadic,
    size_t numSuppliedArgs)
{
    const size_t numParams = genericArgs.Size();

    for (size_t counter = 0; counter < numParams; counter++)
    {
        // variadic keeps counting
        if (!isVariadic && currentIndex == numParams)
        {
            currentIndex = 0;
        }

        // check if index is used
        if (usedIndices.Find(currentIndex) == usedIndices.End())
        {
            // not found, return the index
            return currentIndex;
        }

        currentIndex++;
    }

    // no slot available
    return size_t(-1);
}

size_t SemanticAnalyzer::Helpers::ArgIndex(
    size_t currentIndex,
    const ArgInfo& argInfo,
    const FlatSet<size_t>& usedIndices,
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    bool isVariadic,
    size_t numSuppliedArgs)
{
    if (argInfo.isNamed)
    {
        for (size_t i = 0; i < genericArgs.Size(); i++)
        {
            const String& genericArgName = genericArgs[i].m_name;

            if (genericArgName == argInfo.name && !usedIndices.Contains(i))
            {
                return i;
            }
        }

        return size_t(-1);
    }

    return FindFreeSlot(
        currentIndex,
        usedIndices,
        genericArgs,
        isVariadic,
        numSuppliedArgs);
}

SymbolType* SemanticAnalyzer::Helpers::SubstituteGenericParameters(
    AstVisitor* visitor,
    Module* mod,
    const SymbolType* inputType,
    const Array<GenericInstanceTypeInfo::Arg>& inArgs,
    const SourceLocation& location)
{
    if (!inputType)
    {
        return nullptr;
    }

    const SymbolType* originalInputType = inputType;

    inputType = inputType->GetUnaliased();

    const TypeInstanceCache::Key cacheKey = TypeInstanceCache::MakeKey(inputType, GenericInstanceTypeInfo { inArgs });

    // If we already created (or are in the middle of creating) this instance, return it to avoid recursion
    if (SymbolType* cachedType = mod->LookupTypeInstance(cacheKey))
    {
        return cachedType;
    }

    ScopeGuard scope { mod, SCOPE_TYPE_NORMAL, 0 };

    switch (inputType->GetTypeClass())
    {
    case TYPE_GENERIC_PARAMETER:
    {
        // Resolve the generic parameter via the current module scope (aliases are set by callers for this context)
        const SymbolType* resolved = mod->LookupSymbolType(inputType->GetName());

        if (!resolved)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_generic_parameter_resolution_failed,
                location,
                inputType->GetName()));

            return const_cast<SymbolType*>(BuiltinTypes::s_errorType);
        }

        return const_cast<SymbolType*>(resolved->GetUnaliased());
    }

    case TYPE_GENERIC_INSTANCE:
    {
        // cache to break recursive cycles (members referencing the same type)
        SymbolType* tempInstance = SymbolType::Temp();

        mod->CacheTypeInstance(cacheKey, tempInstance);

        // Add SelfType alias
        SymbolType* selfTypeAlias = SymbolType::Alias("SelfType", AliasTypeInfo { tempInstance });
        selfTypeAlias->Register(visitor->GetCompilationUnit());
        mod->scopeTree.Top().identifierTable.AddSymbolType(selfTypeAlias);

        const GenericInstanceTypeInfo& gi = inputType->GetGenericInstanceInfo();

        const SymbolType* varargElemType = GetVarArgType(gi.m_genericArgs);
        const size_t fixedCount = varargElemType != nullptr
            ? gi.m_genericArgs.Size() - 1
            : gi.m_genericArgs.Size();

        const size_t aliasCount = MathUtil::Min(fixedCount, inArgs.Size());
        for (size_t i = 0; i < aliasCount; ++i)
        {
            const GenericInstanceTypeInfo::Arg& srcArg = gi.m_genericArgs[i];

            Assert(srcArg.m_type != nullptr);
            Assert(inArgs[i].m_type != nullptr);

            SymbolType* genericParamAlias = SymbolType::Alias(srcArg.m_type->GetName(), AliasTypeInfo { inArgs[i].m_type });
            genericParamAlias->Register(visitor->GetCompilationUnit());
            mod->scopeTree.Top().identifierTable.AddSymbolType(genericParamAlias);
        }

        Array<GenericInstanceTypeInfo::Arg> resolvedArgs;
        resolvedArgs.Reserve(MathUtil::Max(gi.m_genericArgs.Size(), inArgs.Size()));

        // fixed parameters (by index)
        for (size_t i = 0; i < fixedCount; ++i)
        {
            const bool hasProvided = i < inArgs.Size();
            const GenericInstanceTypeInfo::Arg& src = gi.m_genericArgs[i];

            GenericInstanceTypeInfo::Arg& dst = resolvedArgs.EmplaceBack();
            dst = {};
            dst.m_name = hasProvided ? inArgs[i].m_name : src.m_name;
            dst.m_isRef = hasProvided ? inArgs[i].m_isRef : src.m_isRef;
            dst.m_isConst = hasProvided ? inArgs[i].m_isConst : src.m_isConst;
            dst.m_defaultValue = CloneAstNode(hasProvided ? inArgs[i].m_defaultValue : src.m_defaultValue);

            const SymbolType* srcType = hasProvided ? inArgs[i].m_type : src.m_type;
            Assert(srcType != nullptr);

            // Substitute recursively with an empty arg list – resolution of generic params uses scoped aliases
            dst.m_type = SubstituteGenericParameters(visitor, mod, srcType, {}, location);
        }

        // variadic parameters (append any remaining supplied args)
        if (varargElemType != nullptr)
        {
            for (size_t i = fixedCount; i < inArgs.Size(); ++i)
            {
                const GenericInstanceTypeInfo::Arg& src = inArgs[i];

                GenericInstanceTypeInfo::Arg& dst = resolvedArgs.EmplaceBack();
                dst = {};
                dst.m_name = src.m_name;
                dst.m_isRef = src.m_isRef;
                dst.m_isConst = src.m_isConst;
                dst.m_defaultValue = CloneAstNode(src.m_defaultValue);

                Assert(src.m_type != nullptr);

                dst.m_type = SubstituteGenericParameters(visitor, mod, src.m_type, {}, location);
            }
        }
        else if (inArgs.Size() > fixedCount)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incorrect_number_of_arguments,
                location,
                gi.m_genericArgs.Size(),
                inArgs.Size()));
        }

        SymbolType* genericInstanceType = SymbolType::GenericInstance(
            inputType,
            Array<SymbolTypeMember>(inputType->GetMembers()),
            Array<SymbolTypeMember>(inputType->GetStaticMembers()),
            GenericInstanceTypeInfo { resolvedArgs });

        // Non-static members
        for (size_t i = 0; i < genericInstanceType->GetMembers().Size(); ++i)
        {
            const SymbolTypeMember& srcMember = genericInstanceType->GetMembers()[i];
            Assert(srcMember.GetType() != nullptr);

            SymbolType* substituted = SubstituteGenericParameters(visitor, mod, srcMember.GetType(), {}, location);

            if (substituted != srcMember.GetType())
            {
                SymbolTypeMember& dstMember = genericInstanceType->GetMembers()[i];
                dstMember.SetType(substituted);
                dstMember.SetExpr(CloneAstNode(srcMember.GetExpr()));
            }
        }

        // Static members
        for (size_t i = 0; i < genericInstanceType->GetStaticMembers().Size(); ++i)
        {
            const SymbolTypeMember& srcMember = genericInstanceType->GetStaticMembers()[i];
            Assert(srcMember.GetType() != nullptr);

            SymbolType* substituted = SubstituteGenericParameters(visitor, mod, srcMember.GetType(), {}, location);

            if (substituted != srcMember.GetType())
            {
                SymbolTypeMember& dstMember = genericInstanceType->GetStaticMembers()[i];
                dstMember.SetType(substituted);
                dstMember.SetExpr(CloneAstNode(srcMember.GetExpr()));
            }
        }

        // Finalize the pre-cached instance in-place and return
        tempInstance->Assign(std::move(*genericInstanceType));

        delete genericInstanceType;

        return tempInstance;
    }
    default:
        break;
    }

    // Types without generic parameters/instances are returned unchanged
    return const_cast<SymbolType*>(inputType);
}

const SymbolType* SemanticAnalyzer::Helpers::GenericPromotion(
    AstVisitor* visitor,
    Module* mod,
    const SymbolType* lptr,
    const SymbolType* rptr,
    const SourceLocation& location)
{
    Assert(lptr != nullptr);
    Assert(rptr != nullptr);

    const SymbolType* lptrUnalias = lptr->GetUnaliased();
    const SymbolType* rptrUnalias = rptr->GetUnaliased();

    switch (lptrUnalias->GetTypeClass())
    {
    case TYPE_GENERIC_INSTANCE:
    {
        // Allows Function to be promoted to Function<T...> if the arguments are compatible
        if (rptrUnalias->GetTypeClass() == TYPE_GENERIC_INSTANCE)
        {
            const SymbolType* lbase = lptrUnalias->GetBaseType();
            const SymbolType* rbase = rptrUnalias->GetBaseType();

            if (lbase && rbase && lbase->TypeEqual(*rbase))
            {
                // they have the same base, use the generic args of rptr to substitute
                const Array<GenericInstanceTypeInfo::Arg>& rGenericArgs = rptrUnalias->GetGenericInstanceInfo().m_genericArgs;
                const SymbolType* promotedType = SubstituteGenericParameters(
                    visitor,
                    mod,
                    lptrUnalias,
                    rGenericArgs,
                    location);

                if (promotedType)
                {
                    promotedType->Register(visitor->GetCompilationUnit());

                    if (promotedType->TypeCompatible(
                            *rptrUnalias,
                            /* strictNumbers */ true,
                            /* strictAny */ true,
                            /* strictEnum */ true,
                            /* strictNull */ true,
                            /* strictDownCasting */ true))
                    {
                        return promotedType->GetUnaliased();
                    }
                }

                return lptr;
            }
        }

        break;
    }
    }

    // no promotion
    return lptr;
}

const SymbolType* SemanticAnalyzer::Helpers::GetVarArgType(const Array<GenericInstanceTypeInfo::Arg>& genericArgs)
{
    if (genericArgs.Empty())
    {
        return nullptr;
    }

    const SymbolType* lastGenericArgType = genericArgs.Back().m_type;
    Assert(lastGenericArgType != nullptr);
    lastGenericArgType = lastGenericArgType->GetUnaliased();

    if (lastGenericArgType->IsVarArgsType())
    {
        if (lastGenericArgType->GetGenericInstanceInfo().m_genericArgs.Any())
        {
            Assert(lastGenericArgType->GetGenericInstanceInfo().m_genericArgs[0].m_type != nullptr);

            return lastGenericArgType->GetGenericInstanceInfo().m_genericArgs[0].m_type;
        }

        return BuiltinTypes::s_anyType;
    }

    return nullptr;
}

void SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolType* symbolType,
    const Array<SharedPtr<AstArgument>>& args,
    const SourceLocation& location)
{
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs = symbolType->GetGenericInstanceInfo().m_genericArgs;

    if (genericArgs.Empty())
    {
        return;
    }

    // make sure the return type exists
    Assert(genericArgs[0].m_type != nullptr);

    const Array<GenericInstanceTypeInfo::Arg> genericArgsWithoutReturn(genericArgs.Begin() + 1, genericArgs.End());

    const SymbolType* varargType = GetVarArgType(genericArgsWithoutReturn);

    const size_t numGenericArgs = varargType != nullptr
        ? genericArgsWithoutReturn.Size() - 1
        : genericArgsWithoutReturn.Size();

    for (size_t index = 0; index < args.Size(); index++)
    {
        const SharedPtr<AstArgument>& arg = args[index];

        if (!arg || arg->IsPlaceholderArgument())
        {
            continue;
        }

        if (index >= numGenericArgs && varargType != nullptr)
        {
            CheckArgTypeCompatible(
                visitor,
                mod,
                arg->GetLocation(),
                arg->GetExprType(),
                varargType);
        }
        else if (index < numGenericArgs && genericArgsWithoutReturn[index].m_type != nullptr)
        {
            CheckArgTypeCompatible(
                visitor,
                mod,
                arg->GetLocation(),
                arg->GetExprType(),
                genericArgsWithoutReturn[index].m_type);
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incorrect_number_of_arguments,
                location,
                genericArgsWithoutReturn.Size(),
                args.Size()));
        }
    }
}

bool SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
    AstVisitor* visitor, Module* mod,
    const SymbolType* symbolType,
    const Array<SharedPtr<AstArgument>>& args,
    const SourceLocation& location,
    const SymbolType*& outReturnType,
    Array<SharedPtr<AstArgument>>& outArgs)
{
    outReturnType = nullptr;

    const Array<GenericInstanceTypeInfo::Arg>& argTypes = symbolType->GetGenericInstanceInfo().m_genericArgs;

    if (argTypes.Empty())
    {
        return false;
    }

    const Array<GenericInstanceTypeInfo::Arg> argTypesWithoutReturn(argTypes.Begin() + 1, argTypes.End());

    // make sure the return type exists
    outReturnType = argTypes[0].m_type;
    Assert(outReturnType != nullptr);

    const SymbolType* varargType = GetVarArgType(argTypesWithoutReturn);

    const size_t numArgs = varargType != nullptr
        ? argTypesWithoutReturn.Size() - 1
        : argTypesWithoutReturn.Size();

    size_t numArgsNoDefaults = numArgs;

    for (size_t index = 0; index < numArgs; ++index)
    {
        if (argTypesWithoutReturn[index].m_defaultValue != nullptr)
        {
            --numArgsNoDefaults;
        }
    }

    FlatSet<size_t> usedIndices;

    Array<SubstitutionResult> substitutionResults;
    substitutionResults.Resize(numArgs);

    if (numArgsNoDefaults <= args.Size())
    {
        struct ArgDataPair
        {
            ArgInfo argInfo;
            SharedPtr<AstArgument> argument;
        };

        Array<ArgDataPair> namedArgs;
        Array<ArgDataPair> unnamedArgs;

        // sort into two separate buckets
        for (size_t i = 0; i < args.Size(); i++)
        {
            Assert(args[i] != nullptr);

            ArgInfo argInfo {};
            argInfo.isNamed = args[i]->IsNamed();
            argInfo.name = args[i]->GetName();
            argInfo.type = nullptr;

            Array<ArgDataPair>& targetArray = argInfo.isNamed ? namedArgs : unnamedArgs;
            targetArray.PushBack(ArgDataPair { argInfo, args[i] });
        }

        // handle named arguments first
        for (size_t i = 0; i < namedArgs.Size(); i++)
        {
            const ArgDataPair& arg = namedArgs[i];
            Assert(arg.argument != nullptr);

            const size_t foundIndex = ArgIndex(
                i,
                arg.argInfo,
                usedIndices,
                argTypesWithoutReturn);

            if (foundIndex != size_t(-1))
            {
                usedIndices.Insert(foundIndex);

                // found successfully, check type compatibility
                // const SymbolTypeRef &paramType = genericArgsWithoutReturn[foundIndex].m_type;

                // CheckArgTypeCompatible(
                //     visitor,
                //     std::get<1>(arg)->GetLocation(),
                //     std::get<0>(arg).type,
                //     paramType
                // );

                Assert(
                    size_t(foundIndex) < substitutionResults.Size(),
                    "Index out of bounds: {} >= {}",
                    foundIndex,
                    substitutionResults.Size());

                arg.argument->SetIsPassByRef(argTypesWithoutReturn[foundIndex].m_isRef);
                arg.argument->SetIsPassConst(argTypesWithoutReturn[foundIndex].m_isConst);

                substitutionResults[foundIndex].value = arg.argument;
                substitutionResults[foundIndex].index = foundIndex;
            }
            else
            {
                // not found so add error
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_named_arg_not_found,
                    arg.argument->GetLocation(),
                    arg.argInfo.name));
            }
        }

        // handle unnamed arguments
        for (size_t i = 0; i < unnamedArgs.Size(); i++)
        {
            const ArgDataPair& arg = unnamedArgs[i];
            Assert(arg.argument != nullptr);

            size_t foundIndex = ArgIndex(
                i,
                arg.argInfo,
                usedIndices,
                argTypesWithoutReturn,
                varargType != nullptr);

            if (varargType != nullptr && ((i + namedArgs.Size())) >= argTypesWithoutReturn.Size() - 1)
            {
                // // in varargs... check against vararg type
                // CheckArgTypeCompatible(
                //     visitor,
                //     mod,
                //     arg.argument->GetLocation(),
                //     arg.argument->GetExprType(),
                //     varargType);

                const bool isRef = argTypesWithoutReturn.Size() != 0 && argTypesWithoutReturn[argTypesWithoutReturn.Size() - 1].m_isRef;
                const bool isConst = argTypesWithoutReturn.Size() != 0 && argTypesWithoutReturn[argTypesWithoutReturn.Size() - 1].m_isConst;

                arg.argument->SetIsPassByRef(isRef);
                arg.argument->SetIsPassConst(isConst);

                if (foundIndex == size_t(-1) || foundIndex >= substitutionResults.Size())
                {
                    foundIndex = substitutionResults.Size();

                    // at end, push to make room
                    substitutionResults.Resize(substitutionResults.Size() + 1);
                }

                Assert(
                    size_t(foundIndex) < substitutionResults.Size(),
                    "Index out of bounds: {} >= {}",
                    foundIndex,
                    substitutionResults.Size());

                usedIndices.Insert(foundIndex);

                substitutionResults[foundIndex] = {
                    arg.argument,
                    foundIndex
                };
            }
            else if (foundIndex != size_t(-1))
            {
                Assert(
                    size_t(foundIndex) < substitutionResults.Size(),
                    "Index out of bounds: {} >= {}",
                    foundIndex,
                    substitutionResults.Size());

                // store used index
                usedIndices.Insert(foundIndex);

                const GenericInstanceTypeInfo::Arg& param = (foundIndex < argTypesWithoutReturn.Size())
                    ? argTypesWithoutReturn[foundIndex]
                    : argTypesWithoutReturn.Back();

                arg.argument->SetIsPassByRef(param.m_isRef);
                arg.argument->SetIsPassConst(param.m_isConst);

                substitutionResults[foundIndex] = {
                    arg.argument,
                    foundIndex
                };
            }
            else
            {
                // too many args supplied
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_incorrect_number_of_arguments,
                    location,
                    argTypesWithoutReturn.Size(),
                    args.Size()));
            }
        }

        // handle arguments that weren't passed in, but have default assignments.
        Array<size_t> unusedIndices;

        for (size_t index = 0; index < numArgs; ++index)
        {
            if (!unusedIndices.Contains(index) && !usedIndices.Contains(index))
            {
                unusedIndices.PushBack(index);
            }
        }

        size_t unusedIndexCounter = 0;

        for (auto it = unusedIndices.Begin(); it != unusedIndices.End();)
        {
            const size_t unusedIndex = *it;

            Assert(unusedIndex < argTypesWithoutReturn.Size());

            const bool hasDefaultValue = argTypesWithoutReturn[unusedIndex].m_defaultValue != nullptr;
            const bool isRef = argTypesWithoutReturn[unusedIndex].m_isRef;
            const bool isConst = argTypesWithoutReturn[unusedIndex].m_isConst;

            SharedPtr<AstArgument> substitutedArg;

            SharedPtr<AstExpression> expr;

            if (hasDefaultValue)
            {
                expr = CloneAstNode(argTypesWithoutReturn[unusedIndex].m_defaultValue);
            }
            else
            {
                expr.Reset(new AstUndefined(location));
            }

            substitutedArg.Reset(new AstArgument(
                expr,
                false,
                true,
                isRef,
                isConst,
                argTypesWithoutReturn[unusedIndex].m_name,
                location));

            expr.Reset();

            // push the default value as argument
            // use named argument, same name as in definition

            substitutedArg->Visit(visitor, mod);

            ArgInfo argInfo;
            argInfo.isNamed = substitutedArg->IsNamed();
            argInfo.name = substitutedArg->GetName();
            argInfo.type = substitutedArg->GetExprType();

            size_t foundIndex = ArgIndex(
                unusedIndexCounter,
                argInfo,
                usedIndices,
                argTypesWithoutReturn);

            if (foundIndex == size_t(-1))
            {
                foundIndex = substitutionResults.Size();
                substitutionResults.Resize(substitutionResults.Size() + 1);
            }

            Assert(size_t(foundIndex) < substitutionResults.Size());
            substitutionResults[foundIndex] = {
                std::move(substitutedArg),
                foundIndex
            };

            if (!hasDefaultValue)
            {
                // not provided and no default value
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_missing_argument,
                    location,
                    argTypesWithoutReturn[unusedIndex].m_name));
            }

            ++unusedIndexCounter;

            it = unusedIndices.Erase(it);
        }

        if (unusedIndices.Any())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incorrect_number_of_arguments,
                location,
                numArgsNoDefaults,
                args.Size()));
        }
    }
    else
    {
        // wrong number of args given
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_incorrect_number_of_arguments,
            location,
            numArgsNoDefaults,
            args.Size()));
    }

    outArgs.Resize(substitutionResults.Size());

    for (const auto& substitutionResult : substitutionResults)
    {
        if (substitutionResult.index == size_t(-1))
        {
            // error occurred during substitution
            continue;
        }

        Assert(substitutionResult.index < outArgs.Size());
        Assert(substitutionResult.value.Is<SharedPtr<AstArgument>>());

        outArgs[substitutionResult.index] = CloneAstNode(substitutionResult.value.Get<SharedPtr<AstArgument>>());
    }

    return true;
}

void SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolType* symbolType,
    const SymbolType* assignmentType,
    bool strictEnum,
    bool strictNull,
    const SourceLocation& location)
{
    Assert(symbolType != nullptr);
    Assert(assignmentType != nullptr);

    const SymbolType* resolvedSymbolType = ResolvePlaceholderType(visitor, mod, symbolType, location);
    Assert(resolvedSymbolType != nullptr);
    resolvedSymbolType->Register(visitor->GetCompilationUnit());

    const SymbolType* resolvedAssignmentType = ResolvePlaceholderType(visitor, mod, assignmentType, location);
    Assert(resolvedAssignmentType != nullptr);
    resolvedAssignmentType->Register(visitor->GetCompilationUnit());

    SymbolTypeIncompatibilities incompatibilities;

    if (!resolvedSymbolType->TypeCompatible(
            *resolvedAssignmentType,
            /* strictNumbers */ true,
            /* strictAny */ true,
            /* strictEnum */ strictEnum,
            /* strictNull */ strictNull,
            /* strictDownCasting */ true,
            &incompatibilities))
    {
        if (incompatibilities.Any())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_mismatched_types_assignment_more_info,
                location,
                resolvedSymbolType->ToString(),
                resolvedAssignmentType->ToString(),
                (incompatibilities.Size() > 1
                     ? "\n\t* " + String::Join(MapToArray(incompatibilities, &SymbolTypeIncompatibility::details), "\n\t* ")
                     : " " + incompatibilities[0].details)));
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_mismatched_types_assignment,
                location,
                resolvedSymbolType->ToString(),
                resolvedAssignmentType->ToString()));
        }
    }
}

SemanticAnalyzer::SemanticAnalyzer(
    AstIterator* astIterator,
    CompilationUnit* compilationUnit)
    : AstVisitor(astIterator, compilationUnit)
{
}

SemanticAnalyzer::SemanticAnalyzer(const SemanticAnalyzer& other)
    : AstVisitor(other.m_astIterator, other.m_compilationUnit)
{
}

void SemanticAnalyzer::Analyze(bool expectModuleDecl)
{
    Module* mod = m_compilationUnit->GetCurrentModule();
    Assert(mod != nullptr);

    while (m_astIterator->HasNext())
    {
        auto node = m_astIterator->Next();
        Assert(node != nullptr);

        // this will not work due to nested Visit() calls
        node->SetScopeDepth(m_compilationUnit->GetCurrentModule()->scopeTree.TopNode()->m_depth);

        node->Visit(this, mod);
    }
}
} // namespace Hyperion
