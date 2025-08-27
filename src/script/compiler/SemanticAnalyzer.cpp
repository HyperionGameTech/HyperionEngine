#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

#include <set>
#include <iostream>
#include <sstream>

namespace hyperion::compiler {

void SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
    AstVisitor* visitor,
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

    if (paramType->TypeCompatible(*argType, true))
    {
        return;
    }

    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_arg_type_incompatible,
        location,
        argType->ToString(),
        paramType->ToString()));
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
    const Array<SubstitutionResult>& substitutionResults,
    const SourceLocation& location)
{
    if (!inputType)
    {
        return nullptr;
    }

    const auto FindArgForInputType = [&]() -> const SubstitutionResult*
    {
        SizeType genericArgIndex = SizeType(-1);

        for (SizeType index = 0; index < genericArgs.Size(); index++)
        {
            if (genericArgs[index].m_name == inputType->GetName())
            {
                genericArgIndex = index;

                break;
            }
        }

        const auto substitutionResultIt = substitutionResults.FindIf([genericArgIndex](const SubstitutionResult& substitutionResult)
            {
                return substitutionResult.index == genericArgIndex;
            });

        if (substitutionResultIt == substitutionResults.End())
        {
            return nullptr;
        }

        return substitutionResultIt;
    };

    switch (inputType->GetTypeClass())
    {
    case TYPE_GENERIC_PARAMETER:
    {
        const SubstitutionResult* foundSubstitutionResult = FindArgForInputType();

        if (foundSubstitutionResult)
        {
            Assert(foundSubstitutionResult->arg != nullptr);

            const GenericInstanceTypeInfo::Arg& genericArg = genericArgs[foundSubstitutionResult->index];

            // @TODO We need to reevaluate the order of which Arguments are visited VS. this chain of methods gets called.
            /// We need a way to mark ref/const BEFORE visiting OR we need to ignore it in the first visiting stage and just use it for building..
            /// // Or we could do first visit, mark ref/const, then clone + visit again.

            foundSubstitutionResult->arg->SetIsPassByRef(genericArg.m_isRef);
            foundSubstitutionResult->arg->SetIsPassConst(genericArg.m_isConst);

            auto* deepValueOf = foundSubstitutionResult->arg->GetDeepValueOf();
            Assert(deepValueOf != nullptr);

            if (SymbolTypeRef heldType = deepValueOf->GetHeldType())
            {
                if (!heldType->IsOrHasBase(*BuiltinTypes::UNDEFINED))
                {
                    return heldType;
                }
            }
        }

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_no_substitution_for_generic_arg,
            location,
            inputType->GetName()));

        return BuiltinTypes::UNDEFINED;
    }
    case TYPE_GENERIC_INSTANCE:
    {
        auto baseType = inputType->GetBaseType();
        Assert(baseType != nullptr);

        const GenericInstanceTypeInfo& genericInstanceInfo = inputType->GetGenericInstanceInfo();

        Array<GenericInstanceTypeInfo::Arg> resArgs = genericInstanceInfo.m_genericArgs;

        for (SizeType index = 0; index < genericInstanceInfo.m_genericArgs.Size(); index++)
        {
            resArgs[index].m_type = SubstituteGenericParameters(
                visitor,
                mod,
                genericInstanceInfo.m_genericArgs[index].m_type,
                genericArgs,
                substitutionResults,
                location);
        }

        SymbolTypeRef substitutedType = SymbolType::GenericInstance(
            baseType,
            GenericInstanceTypeInfo {
                std::move(resArgs) });

        for (const SymbolTypeMember& member : inputType->GetMembers())
        {
            substitutedType->AddMember({ member.name,
                SubstituteGenericParameters(
                    visitor,
                    mod,
                    member.type,
                    genericArgs,
                    substitutionResults,
                    location),
                member.expr });
        }

        return substitutedType;
    }
    default:
        return inputType;
    }
}

Optional<SymbolTypeFunctionSignature> SemanticAnalyzer::Helpers::ExtractGenericArgs(
    AstVisitor* visitor,
    Module* mod,
    const SymbolTypeRef& symbolType,
    const Array<RC<AstArgument>>& args,
    const SourceLocation& location,
    Array<SubstitutionResult> (*fn)(
        AstVisitor* visitor,
        Module* mod,
        const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
        const Array<RC<AstArgument>>& args,
        const SourceLocation& location))
{
    const Array<GenericInstanceTypeInfo::Arg>& genericArgs = symbolType->GetGenericInstanceInfo().m_genericArgs;

    if (genericArgs.Empty())
    {
        return {};
    }

    // make sure the "return type" of the function is not null
    Assert(genericArgs[0].m_type != nullptr);

    const Array<GenericInstanceTypeInfo::Arg> genericArgsWithoutReturn(genericArgs.Begin() + 1, genericArgs.End());

    const auto substitutionResults = fn(
        visitor,
        mod,
        genericArgsWithoutReturn,
        args,
        location);

    for (const auto& substitutionResult : substitutionResults)
    {
        if (!substitutionResult.arg)
        {
            // Error occurred
            return {};
        }
    }

    // replace generics used within the "return type" of the function/generic
    const SymbolTypeRef returnType = SubstituteGenericParameters(
        visitor,
        mod,
        genericArgs[0].m_type,
        genericArgsWithoutReturn,
        substitutionResults,
        location);

    Assert(returnType != nullptr);

    Array<RC<AstArgument>> resArgs;
    resArgs.Reserve(substitutionResults.Size());

    for (const auto& substitutionResult : substitutionResults)
    {
        resArgs.PushBack(substitutionResult.arg);
    }

    return SymbolTypeFunctionSignature {
        returnType,
        resArgs
    };
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
        if (lastGenericArgType->IsGenericParameter())
        {
            return BuiltinTypes::PLACEHOLDER;
        }

        const auto& lastGenericArgTypeArgs = lastGenericArgType->GetGenericInstanceInfo().m_genericArgs;
        Assert(!lastGenericArgTypeArgs.Empty());

        const auto& lastGenericArgTypeArgsFirst = lastGenericArgTypeArgs.Front();
        Assert(lastGenericArgTypeArgsFirst.m_type != nullptr);

        return lastGenericArgTypeArgsFirst.m_type;
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
    ExtractGenericArgs(
        visitor,
        mod,
        symbolType,
        args,
        location,
        [](
            AstVisitor* visitor, Module* mod,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location)
        {
            SymbolTypeRef varargType = GetVarArgType(genericArgs);

            const SizeType numGenericArgs = varargType != nullptr
                ? genericArgs.Size() - 1
                : genericArgs.Size();

            for (SizeType index = 0; index < args.Size(); index++)
            {
                if (index >= numGenericArgs && varargType != nullptr)
                {
                    CheckArgTypeCompatible(
                        visitor,
                        args[index]->GetLocation(),
                        args[index]->GetExprType(),
                        varargType);
                }
                else if (index < numGenericArgs && genericArgs[index].m_type != nullptr)
                {
                    CheckArgTypeCompatible(
                        visitor,
                        args[index]->GetLocation(),
                        args[index]->GetExprType(),
                        genericArgs[index].m_type);
                }
                else
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_incorrect_number_of_arguments,
                        location,
                        genericArgs.Size(),
                        args.Size()));
                }
            }

            Array<SubstitutionResult> substitutionResults;
            substitutionResults.Resize(args.Size());

            for (SizeType index = 0; index < args.Size(); index++)
            {
                substitutionResults[index].arg = args[index];
                substitutionResults[index].index = index;
            }

            return substitutionResults;
        });
}

Optional<SymbolTypeFunctionSignature> SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
    AstVisitor* visitor, Module* mod,
    const SymbolTypeRef& symbolType,
    const Array<RC<AstArgument>>& args,
    const SourceLocation& location)
{
    return ExtractGenericArgs(
        visitor,
        mod,
        symbolType,
        args,
        location,
        [](
            AstVisitor* visitor, Module* mod,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location)
        {
            SymbolTypeRef varargType = GetVarArgType(genericArgs);

            const SizeType numGenericArgs = varargType != nullptr
                ? genericArgs.Size() - 1
                : genericArgs.Size();

            SizeType numGenericArgsWithoutDefaultAssigned = numGenericArgs;

            for (SizeType index = 0; index < numGenericArgs; ++index)
            {
                if (genericArgs[index].m_defaultValue != nullptr)
                {
                    --numGenericArgsWithoutDefaultAssigned;
                }
            }

            FlatSet<SizeType> usedIndices;

            Array<SubstitutionResult> substitutionResults;
            substitutionResults.Resize(numGenericArgs);

            if (numGenericArgsWithoutDefaultAssigned <= args.Size())
            {
                using ArgDataPair = std::pair<ArgInfo, RC<AstArgument>>;

                Array<ArgDataPair> namedArgs;
                Array<ArgDataPair> unnamedArgs;

                // sort into two separate buckets
                for (SizeType i = 0; i < args.Size(); i++)
                {
                    Assert(args[i] != nullptr);

                    ArgInfo argInfo;
                    argInfo.isNamed = args[i]->IsNamed();
                    argInfo.name = args[i]->GetName();

                    ArgDataPair argDataPair = {
                        argInfo,
                        args[i]
                    };

                    if (argInfo.isNamed)
                    {
                        namedArgs.PushBack(std::move(argDataPair));
                    }
                    else
                    {
                        unnamedArgs.PushBack(std::move(argDataPair));
                    }
                }

                // handle named arguments first
                for (SizeType i = 0; i < namedArgs.Size(); i++)
                {
                    const ArgDataPair& arg = namedArgs[i];
                    Assert(std::get<1>(arg) != nullptr);

                    const SizeType foundIndex = ArgIndex(
                        i,
                        std::get<0>(arg),
                        usedIndices,
                        genericArgs);

                    if (foundIndex != SizeType(-1))
                    {
                        usedIndices.Insert(foundIndex);

                        // found successfully, check type compatibility
                        // const SymbolTypeRef &paramType = genericArgs[foundIndex].m_type;

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

                        RC<AstArgument> argType = std::get<1>(arg);
                        Assert(argType != nullptr);

                        substitutionResults[foundIndex].arg = argType;
                        substitutionResults[foundIndex].arg->SetIsPassByRef(genericArgs[foundIndex].m_isRef);
                        substitutionResults[foundIndex].arg->SetIsPassConst(genericArgs[foundIndex].m_isConst);
                        substitutionResults[foundIndex].index = foundIndex;
                    }
                    else
                    {
                        // not found so add error
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_named_arg_not_found,
                            std::get<1>(arg)->GetLocation(),
                            std::get<0>(arg).name));
                    }
                }

                // handle unnamed arguments
                for (SizeType i = 0; i < unnamedArgs.Size(); i++)
                {
                    const ArgDataPair& arg = unnamedArgs[i];
                    Assert(std::get<1>(arg) != nullptr);

                    SizeType foundIndex = ArgIndex(
                        i,
                        std::get<0>(arg),
                        usedIndices,
                        genericArgs,
                        varargType != nullptr);

                    if (varargType != nullptr && ((i + namedArgs.Size())) >= genericArgs.Size() - 1)
                    {
                        // in varargs... check against vararg base type
                        // CheckArgTypeCompatible(
                        //     visitor,
                        //     std::get<1>(arg)->GetLocation(),
                        //     std::get<0>(arg).type,
                        //     varargType
                        // );

                        // check should not be neccessary but added just in case
                        const bool isRef = genericArgs.Size() != 0
                            ? genericArgs[genericArgs.Size() - 1].m_isRef
                            : false;

                        const bool isConst = genericArgs.Size() != 0
                            ? genericArgs[genericArgs.Size() - 1].m_isConst
                            : false;

                        RC<AstArgument> argType = std::get<1>(arg);
                        Assert(argType != nullptr);

                        argType->SetIsPassByRef(isRef);
                        argType->SetIsPassConst(isConst);

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
                            std::move(argType),
                            foundIndex
                        };
                    }
                    else if (foundIndex != SizeType(-1))
                    {
                        DebugLog(LogType::Debug, "Found index : %d, results size: %u\n", foundIndex, substitutionResults.Size());
                        Assert(
                            SizeType(foundIndex) < substitutionResults.Size(),
                            "Index out of bounds: {} >= {}",
                            foundIndex,
                            substitutionResults.Size());

                        // store used index
                        usedIndices.Insert(foundIndex);

                        const GenericInstanceTypeInfo::Arg& param = (foundIndex < genericArgs.Size())
                            ? genericArgs[foundIndex]
                            : genericArgs.Back();

                        RC<AstArgument> argType = std::get<1>(arg);
                        Assert(argType != nullptr);

                        argType->SetIsPassByRef(param.m_isRef);
                        argType->SetIsPassConst(param.m_isConst);

                        substitutionResults[foundIndex] = {
                            std::move(argType),
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
                            genericArgs.Size(),
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

                    Assert(unusedIndex < genericArgs.Size());

                    const bool hasDefaultValue = genericArgs[unusedIndex].m_defaultValue != nullptr;
                    const bool isRef = genericArgs[unusedIndex].m_isRef;
                    const bool isConst = genericArgs[unusedIndex].m_isConst;

                    RC<AstArgument> substitutedArg;

                    {
                        RC<AstExpression> expr;

                        if (hasDefaultValue)
                        {
                            expr = CloneAstNode(genericArgs[unusedIndex].m_defaultValue);
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
                            genericArgs[unusedIndex].m_name,
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
                        genericArgs);

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
                            genericArgs[unusedIndex].m_name));
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

            return substitutionResults;
        });
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

    // symbolType should be the user-specified type
    SymbolTypeRef symbolTypePromoted = SymbolType::GenericPromotion(symbolType, assignmentType);
    Assert(symbolTypePromoted != nullptr);

    // generic not yet promoted to an instance
    if (symbolTypePromoted->GetTypeClass() == TYPE_GENERIC)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_generic_parameters_missing,
            location,
            symbolTypePromoted->ToString(),
            symbolTypePromoted->GetGenericInfo().m_numParameters));
    }

    SymbolTypeRef comparisonType = symbolType;

    SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
        visitor,
        mod,
        comparisonType,
        assignmentType,
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

    if (!symbolType->TypeCompatible(*assignmentType, true))
    {
        CompilerError error(
            LEVEL_ERROR,
            Msg_mismatched_types_assignment,
            location,
            assignmentType->ToString(),
            symbolType->ToString());

        if (assignmentType->IsAnyType())
        {
            error = CompilerError(
                LEVEL_ERROR,
                Msg_implicit_any_mismatch,
                location,
                symbolType->ToString());
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
