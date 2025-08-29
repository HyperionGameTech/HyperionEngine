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

    SymbolTypeRef unaliased = inputType->GetUnaliased();

    if (unaliased->GetTypeClass() != TYPE_PLACEHOLDER)
    {
        return inputType;
    }

    SymbolTypeRef foundType = mod->LookupSymbolType(unaliased->GetName());

    if (foundType)
    {
        return foundType;
    }

    inputType->GetScope()->

        visitor->GetCompilationUnit()
            ->GetErrorList()
            .AddError(CompilerError(
                LEVEL_ERROR,
                Msg_placeholder_resolution_failed,
                location,
                unaliased->GetName()));

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

            if (genericArgName == argInfo.name && usedIndices.Find(i) == usedIndices.End())
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
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
    const SourceLocation& location)
{
    if (!inputType)
    {
        return nullptr;
    }

    switch (inputType->GetTypeClass())
    {
    case TYPE_PLACEHOLDER: // fallthrough
    case TYPE_GENERIC_PARAMETER:
    {
        SymbolTypeRef foundReplacement = mod->LookupSymbolType(inputType->GetName());

        if (foundReplacement)
        {
            return foundReplacement;
        }

        if (inputType->IsPlaceholderType())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_placeholder_resolution_failed,
                location,
                inputType->GetName()));
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_no_substitution_for_generic_arg,
                location,
                inputType->GetName()));
        }

        return BuiltinTypes::UNDEFINED;
    }
    case TYPE_GENERIC_INSTANCE:
    {
        // Open new scope for generic substitution
        ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

        SymbolTypeRef baseType = inputType->GetBaseType();
        SymbolTypeRef substitutedBaseType;

        const GenericInstanceTypeInfo& genericInstanceInfo = inputType->GetGenericInstanceInfo();
        Array<GenericInstanceTypeInfo::Arg> resolvedArgs;

        // resolve args in type
        SymbolTypeRef varargType = GetVarArgType(genericInstanceInfo.m_genericArgs);

        const SizeType numGenericArgs = varargType != nullptr
            ? genericInstanceInfo.m_genericArgs.Size() - 1
            : genericInstanceInfo.m_genericArgs.Size();

        for (SizeType index = 0; index < genericArgs.Size(); index++)
        {
            if (index >= numGenericArgs && varargType != nullptr)
            {
                // it is variadic
                resolvedArgs.PushBack({ HYP_FORMAT("arg{}", index), varargType });
            }
            else if (index < numGenericArgs && genericArgs[index].m_type != nullptr)
            {
                // not variadic, push and bind an alias type so that nested generics work
                resolvedArgs.PushBack({ genericInstanceInfo.m_genericArgs[index].m_name, genericArgs[index].m_type });

                mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(SymbolType::Alias(
                    genericInstanceInfo.m_genericArgs[index].m_name,
                    AliasTypeInfo { genericArgs[index].m_type }));
            }
            else
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_incorrect_number_of_arguments,
                    location,
                    genericInstanceInfo.m_genericArgs.Size(),
                    genericArgs.Size()));
            }
        }

        if (baseType != nullptr)
        {
            substitutedBaseType = SubstituteGenericParameters(
                visitor,
                mod,
                baseType,
                {},
                location);
        }

        SymbolTypeRef substitutedType = SymbolType::GenericInstance(
            inputType->GetName(),
            substitutedBaseType,
            {}, {},
            GenericInstanceTypeInfo { std::move(resolvedArgs) });

        for (const SymbolTypeMember& member : inputType->GetMembers())
        {
            substitutedType->AddMember({ member.name,
                SubstituteGenericParameters(
                    visitor,
                    mod,
                    member.type,
                    {},
                    location),
                member.expr });
        }

        return substitutedType;
    }
    default:
        return inputType;
    }
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

bool SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
    AstVisitor* visitor, Module* mod,
    const SymbolTypeRef& symbolType,
    const Array<RC<AstArgument>>& args,
    const SourceLocation& location,
    SymbolTypeRef& outReturnType,
    Array<RC<AstArgument>>& outArgs)
{
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs = symbolType->GetGenericInstanceInfo().m_genericArgs;

    if (genericArgs.Empty())
    {
        return false;
    }

    // make sure the return type exists
    Assert(genericArgs[0].m_type != nullptr);

    const Array<GenericInstanceTypeInfo::Arg> genericArgsWithoutReturn(genericArgs.Begin() + 1, genericArgs.End());

    SymbolTypeRef varargType = GetVarArgType(genericArgsWithoutReturn);

    const SizeType numGenericArgs = varargType != nullptr
        ? genericArgsWithoutReturn.Size() - 1
        : genericArgsWithoutReturn.Size();

    SizeType numGenericArgsWithoutDefaultAssigned = numGenericArgs;

    for (SizeType index = 0; index < numGenericArgs; ++index)
    {
        if (genericArgsWithoutReturn[index].m_defaultValue != nullptr)
        {
            --numGenericArgsWithoutDefaultAssigned;
        }
    }

    FlatSet<SizeType> usedIndices;

    Array<SubstitutionResult> substitutionResults;
    substitutionResults.Resize(numGenericArgs);

    if (numGenericArgsWithoutDefaultAssigned <= args.Size())
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
                genericArgsWithoutReturn);

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

                arg.argument->SetIsPassByRef(genericArgsWithoutReturn[foundIndex].m_isRef);
                arg.argument->SetIsPassConst(genericArgsWithoutReturn[foundIndex].m_isConst);

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
                genericArgsWithoutReturn,
                varargType != nullptr);

            if (varargType != nullptr && ((i + namedArgs.Size())) >= genericArgsWithoutReturn.Size() - 1)
            {
                // in varargs... check against vararg base type
                // CheckArgTypeCompatible(
                //     visitor,
                //     std::get<1>(arg)->GetLocation(),
                //     std::get<0>(arg).type,
                //     varargType
                // );

                // check should not be neccessary but added just in case
                const bool isRef = genericArgsWithoutReturn.Size() != 0
                    ? genericArgsWithoutReturn[genericArgsWithoutReturn.Size() - 1].m_isRef
                    : false;

                const bool isConst = genericArgsWithoutReturn.Size() != 0
                    ? genericArgsWithoutReturn[genericArgsWithoutReturn.Size() - 1].m_isConst
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

                const GenericInstanceTypeInfo::Arg& param = (foundIndex < genericArgsWithoutReturn.Size())
                    ? genericArgsWithoutReturn[foundIndex]
                    : genericArgsWithoutReturn.Back();

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
                    genericArgsWithoutReturn.Size(),
                    args.Size()));
            }
        }

        // handle arguments that weren't passed in, but have default assignments.
        Array<SizeType> unusedIndices;

        for (SizeType index = 0; index < numGenericArgs; ++index)
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

            Assert(unusedIndex < genericArgsWithoutReturn.Size());

            const bool hasDefaultValue = genericArgsWithoutReturn[unusedIndex].m_defaultValue != nullptr;
            const bool isRef = genericArgsWithoutReturn[unusedIndex].m_isRef;
            const bool isConst = genericArgsWithoutReturn[unusedIndex].m_isConst;

            RC<AstArgument> substitutedArg;

            {
                RC<AstExpression> expr;

                if (hasDefaultValue)
                {
                    expr = CloneAstNode(genericArgsWithoutReturn[unusedIndex].m_defaultValue);
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
                    genericArgsWithoutReturn[unusedIndex].m_name,
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
                genericArgsWithoutReturn);

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
                    Msg_generic_expression_invalid_arguments,
                    location,
                    genericArgsWithoutReturn[unusedIndex].m_name));
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
                numGenericArgsWithoutDefaultAssigned,
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
            numGenericArgsWithoutDefaultAssigned,
            args.Size()));
    }

    // replace generics used within the return type.
    outReturnType = SubstituteGenericParameters(
        visitor,
        mod,
        genericArgsWithoutReturn[0].m_type,
        genericArgsWithoutReturn,
        location);

    Assert(outReturnType != nullptr);

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
        Assert(outArgs[substitutionResult.index] != nullptr);
    }

    return true;
}

void SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& assignmentType,
    const SourceLocation& location)
{
    Assert(symbolType != nullptr);
    Assert(assignmentType != nullptr);

    SymbolTypeRef symbolTypeUnaliased = symbolType->GetUnaliased();
    SymbolTypeRef assignmentTypeUnaliased = assignmentType->GetUnaliased();

    // symbolType should be the user-specified type
    SymbolTypeRef symbolTypePromoted = SymbolType::GenericPromotion(symbolTypeUnaliased, assignmentTypeUnaliased);
    Assert(symbolTypePromoted != nullptr);

    SymbolTypeRef comparisonType = symbolTypeUnaliased;

    SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
        visitor,
        mod,
        comparisonType,
        assignmentTypeUnaliased,
        location);
}

void SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& assignmentType,
    const SourceLocation& location)
{
    Assert(symbolType != nullptr);
    Assert(assignmentType != nullptr);

    SymbolTypeRef symbolTypeUnaliased = symbolType->GetUnaliased();
    SymbolTypeRef assignmentTypeUnaliased = assignmentType->GetUnaliased();

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
