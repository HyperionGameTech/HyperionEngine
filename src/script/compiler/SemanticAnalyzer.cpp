#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <set>
#include <iostream>
#include <sstream>

namespace hyperion::compiler {

HYP_DISABLE_OPTIMIZATION;
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

    if (!unaliasedInputType->IsPlaceholderType())
    {
        // Check if type has members, static members, or base types that need resolution
        if (unaliasedInputType->GetMembers().Any() || unaliasedInputType->GetStaticMembers().Any() || inputType->GetBaseType())
        {
            if (unaliasedInputType->GetBaseType())
            {
                SymbolTypeRef resolvedBaseType = ResolvePlaceholderType(visitor, mod, inputType->GetBaseType(), location);
                if (resolvedBaseType != unaliasedInputType->GetBaseType())
                {
                    unaliasedInputType->SetBaseType(resolvedBaseType);
                }
            }

            for (SizeType memberIndex = 0; memberIndex < unaliasedInputType->GetMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = unaliasedInputType->GetMembers()[memberIndex];

                SymbolTypeRef resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.type, location);
                if (srcMember.type != resolvedMemberType)
                {
                    unaliasedInputType->GetMembers()[memberIndex] = { srcMember.name, resolvedMemberType, srcMember.expr };
                }
            }

            for (SizeType memberIndex = 0; memberIndex < unaliasedInputType->GetStaticMembers().Size(); memberIndex++)
            {
                const SymbolTypeMember& srcMember = unaliasedInputType->GetStaticMembers()[memberIndex];

                SymbolTypeRef resolvedMemberType = ResolvePlaceholderType(visitor, mod, srcMember.type, location);
                if (srcMember.type != resolvedMemberType)
                {
                    unaliasedInputType->GetStaticMembers()[memberIndex] = { srcMember.name, resolvedMemberType, srcMember.expr };
                }
            }
        }

        return unaliasedInputType;
    }

    // Try to resolve placeholder type by looking it up in the module
    SymbolTypeRef resolvedType = mod->LookupSymbolType(unaliasedInputType->GetName());

    if (resolvedType)
    {
        return resolvedType;
    }

    // Could not resolve placeholder type
    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_placeholder_resolution_failed,
        location,
        inputType->GetName()));

    return BuiltinTypes::UNDEFINED;
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

    // make sure argument types are compatible
    // use strict numbers so that floats cannot be passed as explicit ints
    // @NOTE: do not add error for undefined, it causes too many unnecessary errors
    //        that would've already been conveyed via 'not declared' errors
    if (argType == BuiltinTypes::UNDEFINED)
    {
        return;
    }

    SymbolTypeRef resolvedParamType = ResolvePlaceholderType(visitor, mod, paramType, location);
    Assert(resolvedParamType != nullptr);

    SymbolTypeRef resolvedArgType = ResolvePlaceholderType(visitor, mod, argType, location);
    Assert(resolvedArgType != nullptr);

    if (resolvedParamType->TypeCompatible(*resolvedArgType, true))
    {
        return;
    }

    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_arg_type_incompatible,
        location,
        resolvedArgType->ToString(),
        resolvedParamType->ToString()));
}
HYP_ENABLE_OPTIMIZATION;

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

            if (genericArgName == argInfo.name
                && usedIndices.Find(i) == usedIndices.End())
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

HYP_DISABLE_OPTIMIZATION;
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

    if (unaliasedInputType->IsPlaceholderType())
    {
        return ResolvePlaceholderType(visitor, mod, unaliasedInputType, location);
    }

    SymbolTypeRef targetType = unaliasedInputType;
    bool changed = false;

    ScopeGuard scope { mod, SCOPE_TYPE_NORMAL, 0 };

    switch (targetType->GetTypeClass())
    {
    case TYPE_PLACEHOLDER:
        HYP_FAIL("Should not encounter placeholder type here");
        return BuiltinTypes::UNDEFINED;
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

            targetType = BuiltinTypes::UNDEFINED;
        }

        targetType = targetType->GetUnaliased();
        Assert(targetType != nullptr);

        DebugLog(LogType::Debug, "Substituted generic parameter %s with type %s\n",
            inputType->GetName().Data(),
            targetType->ToString().Data());

        changed = true;

        break;
    }
    case TYPE_GENERIC_INSTANCE:
    {
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

            mod->m_scopes.Top().identifierTable.AddSymbolType(SymbolType::Alias(
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

        SymbolTypeRef substitutedType = SymbolType::GenericInstance(
            targetType,
            {}, {},
            GenericInstanceTypeInfo { std::move(resolvedArgs) });

        targetType = substitutedType;
        changed = true;

        break;
    }
    default:
        break;
    }

    if (SymbolTypeRef baseType = unaliasedInputType->GetBaseType())
    {
        SymbolTypeRef substitutedBaseType = SubstituteGenericParameters(visitor, mod, baseType, {}, location);

        if (substitutedBaseType != baseType)
        {
            if (!changed)
            {
                targetType = targetType->Clone();
                changed = true;
            }

            targetType->SetBaseType(substitutedBaseType);
        }
    }

    if (changed)
    {
        targetType->GetMembers().Clear();
        targetType->GetStaticMembers().Clear();
    }

    // members
    for (const SymbolTypeMember& member : unaliasedInputType->GetMembers())
    {
        if (!changed)
        {
            targetType = targetType->Clone();
            changed = true;
        }

        targetType->GetMembers().PushBack({ member.name,
            SubstituteGenericParameters(visitor, mod, member.type, {}, location),
            member.expr });
    }

    // static members
    for (const SymbolTypeMember& member : unaliasedInputType->GetStaticMembers())
    {
        if (!changed)
        {
            targetType = targetType->Clone();
            changed = true;
        }

        targetType->GetStaticMembers().PushBack({ member.name,
            SubstituteGenericParameters(visitor, mod, member.type, {}, location),
            member.expr });
    }

    return changed ? targetType : inputType;
}
HYP_ENABLE_OPTIMIZATION;

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

                if (promoted && promoted->TypeCompatible(*rptrUnalias, true))
                {
                    return promoted->GetUnaliased();
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
        SymbolTypeRef baseType = lastGenericArgType->GetBaseType();

        if (baseType)
        {
            return baseType->GetUnaliased();
        }

        return BuiltinTypes::ANY;
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
        if (!args[index])
        {
            continue;
        }

        if (index >= numGenericArgs && varargType != nullptr)
        {
            CheckArgTypeCompatible(
                visitor,
                mod,
                args[index]->GetLocation(),
                args[index]->GetExprType(),
                varargType);
        }
        else if (index < numGenericArgs && genericArgsWithoutReturn[index].m_type != nullptr)
        {
            CheckArgTypeCompatible(
                visitor,
                mod,
                args[index]->GetLocation(),
                args[index]->GetExprType(),
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

HYP_DISABLE_OPTIMIZATION;
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
                // in varargs... check against vararg base type
                // CheckArgTypeCompatible(
                //     visitor,
                //     std::get<1>(arg)->GetLocation(),
                //     std::get<0>(arg).type,
                //     varargType
                // );

                // check should not be neccessary but added just in case
                const bool isRef = argTypesWithoutReturn.Size() != 0
                    ? argTypesWithoutReturn[argTypesWithoutReturn.Size() - 1].m_isRef
                    : false;

                const bool isConst = argTypesWithoutReturn.Size() != 0
                    ? argTypesWithoutReturn[argTypesWithoutReturn.Size() - 1].m_isConst
                    : false;

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

            {
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
            }

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
HYP_ENABLE_OPTIMIZATION;

void SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& assignmentType,
    const SourceLocation& location)
{
    Assert(symbolType != nullptr);
    Assert(assignmentType != nullptr);

    SymbolTypeRef symbolTypeUnaliased = ResolvePlaceholderType(visitor, mod, symbolType->GetUnaliased(), location);
    Assert(symbolTypeUnaliased != nullptr);

    SymbolTypeRef assignmentTypeUnaliased = ResolvePlaceholderType(visitor, mod, assignmentType->GetUnaliased(), location);
    Assert(assignmentTypeUnaliased != nullptr);

    if (!symbolTypeUnaliased->TypeCompatible(*assignmentTypeUnaliased, true))
    {
        CompilerError error(
            LEVEL_ERROR,
            Msg_mismatched_types_assignment,
            location,
            assignmentTypeUnaliased->ToString(),
            symbolTypeUnaliased->ToString());

        if (assignmentTypeUnaliased->IsAnyType())
        {
            error = CompilerError(
                LEVEL_ERROR,
                Msg_implicit_any_mismatch,
                location,
                symbolTypeUnaliased->ToString());
        }

        visitor->GetCompilationUnit()->GetErrorList().AddError(error);
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

        // this will not work due to nseted Visit() calls
        node->SetScopeDepth(m_compilationUnit->GetCurrentModule()->m_scopes.TopNode()->m_depth);

        node->Visit(this, mod);
    }
}

} // namespace hyperion::compiler
