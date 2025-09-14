#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

SymbolTypeRef SemanticAnalyzer::Helpers::ResolvePlaceholderType(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& inputType,
    const SourceLocation& location)
{
    if (!inputType)
    {
        return nullptr;
    }

    SymbolTypeRef unaliasedInputType = inputType->GetUnaliased();
    Assert(unaliasedInputType != nullptr);

    const TypeInstanceCache::Key key = TypeInstanceCache::MakeKey(unaliasedInputType, {});

    SymbolTypeRef cachedPlaceholderType = mod->LookupTypeInstance(key, /* deep */ false);

    if (cachedPlaceholderType)
    {
        // to prevent infinite recursion when resolving placeholder types that reference themselves
        // (e.g members)
        return cachedPlaceholderType;
    }
    bool changed = false;

    // cache the placeholder type before resolving members to prevent infinite recursion
    mod->CacheTypeInstance(key, unaliasedInputType);

    if (!unaliasedInputType->IsPlaceholderType())
    {
        SymbolTypeRef newType;

        // Check if type has members, static members, or base types that need resolution
        if (unaliasedInputType->GetMembers().Any() || unaliasedInputType->GetStaticMembers().Any() || unaliasedInputType->GetBaseType())
        {
            newType = unaliasedInputType->Clone();

            if (SymbolTypeRef baseType = newType->GetBaseType())
            {
                SymbolTypeRef resolvedBaseType = ResolvePlaceholderType(visitor, mod, baseType, location);
                if (!resolvedBaseType->TypeEqual(*baseType))
                {
                    newType->SetBaseType(resolvedBaseType);
                    changed = true;
                }
            }

            for (SizeType memberIndex = 0; memberIndex < newType->GetMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = newType->GetMembers()[memberIndex];

                SymbolTypeRef resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.type, location);
                if (!srcMember.type->TypeEqual(*resolvedMemberType))
                {
                    newType->GetMembers()[memberIndex] = { srcMember.name, resolvedMemberType, srcMember.expr };
                    changed = true;
                }
            }

            for (SizeType memberIndex = 0; memberIndex < newType->GetStaticMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = newType->GetStaticMembers()[memberIndex];

                SymbolTypeRef resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.type, location);
                if (!srcMember.type->TypeEqual(*resolvedMemberType))
                {
                    newType->GetStaticMembers()[memberIndex] = { srcMember.name, resolvedMemberType, srcMember.expr };
                    changed = true;
                }
            }
        }

        if (changed)
        {
            unaliasedInputType->Assign(*newType);
        }

        return unaliasedInputType;
    }

    // Try to resolve placeholder type by looking it up in the module
    SymbolTypeRef resolvedType = mod->LookupSymbolType(unaliasedInputType->GetName());

    if (resolvedType)
    {
        unaliasedInputType->Assign(*resolvedType);

        return unaliasedInputType;
    }

    // Could not resolve placeholder type
    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_placeholder_resolution_failed,
        location,
        inputType->GetName()));

    return BuiltinTypes::s_errorType;
}

void SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
    AstVisitor* visitor,
    Module* mod,
    const SourceLocation& location,
    const SymbolTypeRef& argType,
    const SymbolTypeRef& paramType)
{
    Assert(argType != nullptr);
    Assert(paramType != nullptr);

    if (argType == BuiltinTypes::s_errorType)
    {
        return;
    }

    SymbolTypeRef resolvedParamType = ResolvePlaceholderType(visitor, mod, paramType, location);
    Assert(resolvedParamType != nullptr);

    SymbolTypeRef resolvedArgType = ResolvePlaceholderType(visitor, mod, argType, location);
    Assert(resolvedArgType != nullptr);

    SymbolTypeIncompatibilities incompatibilities;

    if (resolvedParamType->TypeCompatible(
            *resolvedArgType,
            /* strictNumbers */ true,
            /* strictAny */ true,
            /* strictEnum */ true,
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
                    ? "\n\t* " + String::Join(Map(incompatibilities, &SymbolTypeIncompatibility::details), "\n\t* ")
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

SizeType SemanticAnalyzer::Helpers::FindFreeSlot(
    SizeType currentIndex,
    const FlatSet<SizeType>& usedIndices,
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    bool isVariadic,
    SizeType numSuppliedArgs)
{
    const SizeType numParams = genericArgs.Size();

    for (SizeType counter = 0; counter < numParams; counter++)
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
    return SizeType(-1);
}

SizeType SemanticAnalyzer::Helpers::ArgIndex(
    SizeType currentIndex,
    const ArgInfo& argInfo,
    const FlatSet<SizeType>& usedIndices,
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    bool isVariadic,
    SizeType numSuppliedArgs)
{
    if (argInfo.isNamed)
    {
        for (SizeType i = 0; i < genericArgs.Size(); i++)
        {
            const String& genericArgName = genericArgs[i].m_name;

            if (genericArgName == argInfo.name && !usedIndices.Contains(i))
            {
                return i;
            }
        }

        return SizeType(-1);
    }

    return FindFreeSlot(
        currentIndex,
        usedIndices,
        genericArgs,
        isVariadic,
        numSuppliedArgs);
}

SymbolTypeRef SemanticAnalyzer::Helpers::SubstituteGenericParameters(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& inputType,
    const Array<GenericInstanceTypeInfo::Arg>& inArgs,
    const SourceLocation& location)
{
    if (!inputType)
    {
        return nullptr;
    }

    SymbolTypeRef unaliasedInputType = inputType->GetUnaliased();
    Assert(unaliasedInputType != nullptr);

    const TypeInstanceCache::Key cacheKey = TypeInstanceCache::MakeKey(unaliasedInputType, GenericInstanceTypeInfo { inArgs });

    if (SymbolTypeRef cachedType = mod->LookupTypeInstance(cacheKey))
    {
        return cachedType;
    }

    SymbolTypeRef targetType = unaliasedInputType;

    bool changed = false;

    SymbolTypeRef newType;

    ScopeGuard scope { mod, SCOPE_TYPE_NORMAL, 0 };

    switch (targetType->GetTypeClass())
    {
    case TYPE_PLACEHOLDER: // fallthrough
    case TYPE_BUILTIN:
        return targetType;
    case TYPE_GENERIC_PARAMETER:
    {
        targetType = mod->LookupSymbolType(targetType->GetName());

        if (!targetType)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_generic_parameter_resolution_failed,
                location,
                inputType->GetName()));

            targetType = BuiltinTypes::s_errorType;
        }

        targetType = targetType->GetUnaliased();
        Assert(targetType != nullptr);

        changed = true;

        break;
    }
    case TYPE_GENERIC_INSTANCE:
    {
        newType = SymbolType::Temp();
        mod->CacheTypeInstance(cacheKey, newType);

        const GenericInstanceTypeInfo& genericInstanceInfo = targetType->GetGenericInstanceInfo();

        // resolve args in type
        SymbolTypeRef varargType = GetVarArgType(genericInstanceInfo.m_genericArgs);

        const SizeType numGenericArgs = varargType != nullptr
            ? genericInstanceInfo.m_genericArgs.Size() - 1
            : genericInstanceInfo.m_genericArgs.Size();

        for (SizeType index = 0; index < inArgs.Size(); index++)
        {
            Assert(inArgs[index].m_type != nullptr);

            if (index >= numGenericArgs)
            {
                break;
            }

            mod->scopeTree.Top().identifierTable.AddSymbolType(SymbolType::Alias(
                genericInstanceInfo.m_genericArgs[index].m_type->GetName(),
                AliasTypeInfo { inArgs[index].m_type }));
        }

        int numLeftoverArgs = int(genericInstanceInfo.m_genericArgs.Size());

        Array<GenericInstanceTypeInfo::Arg> resolvedArgs;

        for (SizeType index = 0; index < inArgs.Size(); index++, numLeftoverArgs--)
        {
            Assert(inArgs[index].m_type != nullptr);

            if (index >= numGenericArgs && varargType != nullptr)
            {
                GenericInstanceTypeInfo::Arg& resolvedArg = resolvedArgs.EmplaceBack();
                resolvedArg = {};
                resolvedArg.m_name = inArgs[index].m_name;
                resolvedArg.m_type = SubstituteGenericParameters(visitor, mod, inArgs[index].m_type, {}, location);
                resolvedArg.m_defaultValue = CloneAstNode(inArgs[index].m_defaultValue);
                resolvedArg.m_isRef = inArgs[index].m_isRef;
                resolvedArg.m_isConst = inArgs[index].m_isConst;

                continue;
            }

            if (index < numGenericArgs)
            {
                GenericInstanceTypeInfo::Arg& resolvedArg = resolvedArgs.EmplaceBack();
                resolvedArg = {};
                resolvedArg.m_name = inArgs[index].m_name;
                resolvedArg.m_type = SubstituteGenericParameters(visitor, mod, inArgs[index].m_type, {}, location);
                resolvedArg.m_defaultValue = CloneAstNode(inArgs[index].m_defaultValue);
                resolvedArg.m_isRef = inArgs[index].m_isRef;
                resolvedArg.m_isConst = inArgs[index].m_isConst;

                continue;
            }

            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_incorrect_number_of_arguments,
                location,
                genericInstanceInfo.m_genericArgs.Size(),
                inArgs.Size()));
        }

        if (numLeftoverArgs > 0)
        {
            // fill in remaining args with defaults
            for (SizeType index = inArgs.Size(); index < genericInstanceInfo.m_genericArgs.Size(); index++)
            {
                const GenericInstanceTypeInfo::Arg& genericArg = genericInstanceInfo.m_genericArgs[index];

                Assert(genericArg.m_type != nullptr);

                // check is var arg
                if (genericArg.m_type->IsVarArgsType())
                {
                    break;
                }

                GenericInstanceTypeInfo::Arg& resolvedArg = resolvedArgs.EmplaceBack();
                resolvedArg = {};
                resolvedArg.m_name = genericArg.m_name;
                resolvedArg.m_defaultValue = CloneAstNode(genericArg.m_defaultValue);
                resolvedArg.m_type = SubstituteGenericParameters(visitor, mod, genericArg.m_type, {}, location);
                resolvedArg.m_isRef = genericArg.m_isRef;
                resolvedArg.m_isConst = genericArg.m_isConst;
            }
        }

        SymbolTypeRef tmpGenericInstance = SymbolType::GenericInstance(
            targetType,
            targetType->GetMembers(),
            targetType->GetStaticMembers(),
            GenericInstanceTypeInfo { std::move(resolvedArgs) });

        targetType = std::move(tmpGenericInstance);
        changed = true;

#if 1
        // handle members
        for (SizeType memberIndex = 0; memberIndex < targetType->GetMembers().Size(); memberIndex++)
        {
            Assert(memberIndex < targetType->GetMembers().Size());

            const SymbolTypeMember& member = targetType->GetMembers()[memberIndex];

            SymbolTypeRef memberType = member.type->GetUnaliased();
            Assert(memberType != nullptr);

            SymbolTypeRef substitutedMemberType = SubstituteGenericParameters(visitor, mod, memberType, {}, location);

            if (substitutedMemberType == memberType)
            {
                // no change, skip
                continue;
            }

            if (!changed)
            {
                targetType = targetType->Clone();

                newType = SymbolType::Temp();
                mod->CacheTypeInstance(cacheKey, newType);

                changed = true;
            }

            SymbolTypeMember& newMember = targetType->GetMembers()[memberIndex];
            newMember.type = substitutedMemberType;
            newMember.expr = CloneAstNode(member.expr);
        }

        // handle static members
        for (SizeType memberIndex = 0; memberIndex < targetType->GetStaticMembers().Size(); memberIndex++)
        {
            Assert(memberIndex < targetType->GetStaticMembers().Size());

            const SymbolTypeMember& member = targetType->GetStaticMembers()[memberIndex];

            SymbolTypeRef memberType = member.type->GetUnaliased();
            Assert(memberType != nullptr);

            SymbolTypeRef substitutedMemberType = SubstituteGenericParameters(visitor, mod, memberType, {}, location);

            if (substitutedMemberType == memberType)
            {
                // no change, skip
                continue;
            }

            if (!changed)
            {
                targetType = targetType->Clone();

                newType = SymbolType::Temp();
                mod->CacheTypeInstance(cacheKey, newType);

                changed = true;
            }

            SymbolTypeMember& newMember = targetType->GetStaticMembers()[memberIndex];
            newMember.type = substitutedMemberType;
            newMember.expr = CloneAstNode(member.expr);
        }
#endif

        break;
    }
    default:
        break;
    }

    if (newType != nullptr)
    {
        Assert(newType != nullptr);

        // modify newType in place (we already cached it so we need to modify it)
        newType->Assign(*targetType);

        return newType;
    }

    if (changed)
    {
        return targetType;
    }

    // if nothing has been changed, return original type we started with.
    // - since we use equality checks to determine if a type has changed,
    //   we need to return the original input type if we didn't return a new / modified type.
    return inputType;
}

SymbolTypeRef SemanticAnalyzer::Helpers::GenericPromotion(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& lptr,
    const SymbolTypeRef& rptr,
    const SourceLocation& location)
{
    Assert(lptr != nullptr);
    Assert(rptr != nullptr);

    SymbolTypeRef lptrUnalias = lptr->GetUnaliased();
    SymbolTypeRef rptrUnalias = rptr->GetUnaliased();

    switch (lptrUnalias->GetTypeClass())
    {
    case TYPE_GENERIC_INSTANCE:
    {
        // Allows Function to be promoted to Function<T...> if the arguments are compatible
        if (rptrUnalias->GetTypeClass() == TYPE_GENERIC_INSTANCE)
        {
            SymbolTypeRef lbase = lptrUnalias->GetBaseType();
            SymbolTypeRef rbase = rptrUnalias->GetBaseType();

            if (lbase && rbase && lbase->TypeEqual(*rbase))
            {
                // they have the same base, use the generic args of rptr to substitute
                const Array<GenericInstanceTypeInfo::Arg>& rGenericArgs = rptrUnalias->GetGenericInstanceInfo().m_genericArgs;
                SymbolTypeRef promoted = SubstituteGenericParameters(
                    visitor,
                    mod,
                    lptrUnalias,
                    rGenericArgs,
                    location);

                if (promoted)
                {
                    if (promoted->TypeCompatible(
                            *rptrUnalias,
                            /* strictNumbers */ true,
                            /* strictAny */ true,
                            /* strictEnum */ true))
                    {
                        return promoted->GetUnaliased();
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

SymbolTypeRef SemanticAnalyzer::Helpers::GetVarArgType(const Array<GenericInstanceTypeInfo::Arg>& genericArgs)
{
    if (genericArgs.Empty())
    {
        return nullptr;
    }

    SymbolTypeRef lastGenericArgType = genericArgs.Back().m_type;
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
    const SymbolTypeRef& symbolType,
    const Array<RC<AstArgument>>& args,
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

    SymbolTypeRef varargType = GetVarArgType(genericArgsWithoutReturn);

    const SizeType numGenericArgs = varargType != nullptr
        ? genericArgsWithoutReturn.Size() - 1
        : genericArgsWithoutReturn.Size();

    for (SizeType index = 0; index < args.Size(); index++)
    {
        const RC<AstArgument>& arg = args[index];

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
    const SymbolTypeRef& symbolType,
    const Array<RC<AstArgument>>& args,
    const SourceLocation& location,
    SymbolTypeRef& outReturnType,
    Array<RC<AstArgument>>& outArgs)
{
    const Array<GenericInstanceTypeInfo::Arg>& argTypes = symbolType->GetGenericInstanceInfo().m_genericArgs;

    if (argTypes.Empty())
    {
        return false;
    }

    const Array<GenericInstanceTypeInfo::Arg> argTypesWithoutReturn(argTypes.Begin() + 1, argTypes.End());

    // make sure the return type exists
    outReturnType = argTypes[0].m_type;
    Assert(outReturnType != nullptr);

    SymbolTypeRef varargType = GetVarArgType(argTypesWithoutReturn);

    const SizeType numArgs = varargType != nullptr
        ? argTypesWithoutReturn.Size() - 1
        : argTypesWithoutReturn.Size();

    SizeType numArgsNoDefaults = numArgs;

    for (SizeType index = 0; index < numArgs; ++index)
    {
        if (argTypesWithoutReturn[index].m_defaultValue != nullptr)
        {
            --numArgsNoDefaults;
        }
    }

    FlatSet<SizeType> usedIndices;

    Array<SubstitutionResult> substitutionResults;
    substitutionResults.Resize(numArgs);

    if (numArgsNoDefaults <= args.Size())
    {
        struct ArgDataPair
        {
            ArgInfo argInfo;
            RC<AstArgument> argument;
        };

        Array<ArgDataPair> namedArgs;
        Array<ArgDataPair> unnamedArgs;

        // sort into two separate buckets
        for (SizeType i = 0; i < args.Size(); i++)
        {
            Assert(args[i] != nullptr);

            ArgInfo argInfo {};
            argInfo.isNamed = args[i]->IsNamed();
            argInfo.name = args[i]->GetName();

            Array<ArgDataPair>& targetArray = argInfo.isNamed ? namedArgs : unnamedArgs;
            targetArray.PushBack(ArgDataPair { argInfo, args[i] });
        }

        // handle named arguments first
        for (SizeType i = 0; i < namedArgs.Size(); i++)
        {
            const ArgDataPair& arg = namedArgs[i];
            Assert(arg.argument != nullptr);

            const SizeType foundIndex = ArgIndex(
                i,
                arg.argInfo,
                usedIndices,
                argTypesWithoutReturn);

            if (foundIndex != SizeType(-1))
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
                    SizeType(foundIndex) < substitutionResults.Size(),
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
        for (SizeType i = 0; i < unnamedArgs.Size(); i++)
        {
            const ArgDataPair& arg = unnamedArgs[i];
            Assert(arg.argument != nullptr);

            SizeType foundIndex = ArgIndex(
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

                if (foundIndex == SizeType(-1) || foundIndex >= substitutionResults.Size())
                {
                    foundIndex = substitutionResults.Size();

                    // at end, push to make room
                    substitutionResults.Resize(substitutionResults.Size() + 1);
                }

                Assert(
                    SizeType(foundIndex) < substitutionResults.Size(),
                    "Index out of bounds: {} >= {}",
                    foundIndex,
                    substitutionResults.Size());

                usedIndices.Insert(foundIndex);

                substitutionResults[foundIndex] = {
                    arg.argument,
                    foundIndex
                };
            }
            else if (foundIndex != SizeType(-1))
            {
                Assert(
                    SizeType(foundIndex) < substitutionResults.Size(),
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
        Array<SizeType> unusedIndices;

        for (SizeType index = 0; index < numArgs; ++index)
        {
            if (!unusedIndices.Contains(index) && !usedIndices.Contains(index))
            {
                unusedIndices.PushBack(index);
            }
        }

        SizeType unusedIndexCounter = 0;

        for (auto it = unusedIndices.Begin(); it != unusedIndices.End();)
        {
            const SizeType unusedIndex = *it;

            Assert(unusedIndex < argTypesWithoutReturn.Size());

            const bool hasDefaultValue = argTypesWithoutReturn[unusedIndex].m_defaultValue != nullptr;
            const bool isRef = argTypesWithoutReturn[unusedIndex].m_isRef;
            const bool isConst = argTypesWithoutReturn[unusedIndex].m_isConst;

            RC<AstArgument> substitutedArg;

            RC<AstExpression> expr;

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

            SizeType foundIndex = ArgIndex(
                unusedIndexCounter,
                argInfo,
                usedIndices,
                argTypesWithoutReturn);

            if (foundIndex == SizeType(-1))
            {
                foundIndex = substitutionResults.Size();
                substitutionResults.Resize(substitutionResults.Size() + 1);
            }

            Assert(SizeType(foundIndex) < substitutionResults.Size());
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
        if (substitutionResult.index == SizeType(-1))
        {
            // error occurred during substitution
            continue;
        }

        Assert(substitutionResult.index < outArgs.Size());
        Assert(substitutionResult.value.Is<RC<AstArgument>>());

        outArgs[substitutionResult.index] = CloneAstNode(substitutionResult.value.Get<RC<AstArgument>>());
    }

    return true;
}

void SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& assignmentType,
    bool strictEnum,
    const SourceLocation& location)
{
    Assert(symbolType != nullptr);
    Assert(assignmentType != nullptr);

    SymbolTypeRef symbolTypeUnaliased = ResolvePlaceholderType(visitor, mod, symbolType->GetUnaliased(), location);
    Assert(symbolTypeUnaliased != nullptr);

    SymbolTypeRef assignmentTypeUnaliased = ResolvePlaceholderType(visitor, mod, assignmentType->GetUnaliased(), location);
    Assert(assignmentTypeUnaliased != nullptr);

    SymbolTypeIncompatibilities incompatibilities;

    if (!symbolTypeUnaliased->TypeCompatible(
            *assignmentTypeUnaliased,
            /* strictNumbers */ true,
            /* strictAny */ true,
            /* strictEnum */ strictEnum,
            &incompatibilities))
    {
        if (incompatibilities.Any())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_mismatched_types_assignment_more_info,
                location,
                symbolTypeUnaliased->ToString(),
                assignmentTypeUnaliased->ToString(),
                (incompatibilities.Size() > 1
                        ? "\n\t* " + String::Join(Map(incompatibilities, &SymbolTypeIncompatibility::details), "\n\t* ")
                        : " " + incompatibilities[0].details)));
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_mismatched_types_assignment,
                location,
                symbolTypeUnaliased->ToString(),
                assignmentTypeUnaliased->ToString()));
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
} // namespace hyperion
