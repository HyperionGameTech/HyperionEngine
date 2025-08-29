#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/Scope.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

#include <iostream>

namespace hyperion::compiler {

AstVariable::AstVariable(
    const String& name,
    const SourceLocation& location)
    : AstIdentifier(name, location),
      m_shouldInline(false),
      m_isInRefAssignment(false),
      m_isInConstAssignment(false)
{
}

void AstVariable::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_isVisited);
    m_isVisited = true;

    AstIdentifier::Visit(visitor, mod);

    Assert(m_properties.GetIdentifierType() != IDENTIFIER_TYPE_UNKNOWN);

    switch (m_properties.GetIdentifierType())
    {
    case IDENTIFIER_TYPE_VARIABLE:
    {
        Assert(m_properties.GetIdentifier() != nullptr);

        // if alias or const, load direct value.
        // if it's an alias then it will just refer to whatever other variable
        // is being referenced. if it is const, load the direct value held in the variable
        const bool isAlias = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_ALIAS;
        const bool isMixin = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_MIXIN;
        const bool isGeneric = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_GENERIC;
        const bool isArgument = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_ARGUMENT;
        const bool isConst = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_CONST;
        const bool isRef = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_REF;
        const bool isMember = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_MEMBER;
        const bool isSubstitution = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_GENERIC_SUBSTITUTION;

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
        // clone the AST node so we don't double-visit
        if (auto currentValue = m_properties.GetIdentifier()->GetCurrentValue())
        {
            const AstConstant* constantValue = nullptr;

            if (currentValue->IsLiteral() && (constantValue = dynamic_cast<const AstConstant*>(currentValue->GetDeepValueOf())))
            {
                m_inlineValue = CloneAstNode(constantValue);
            }
        }
#endif

        if (false)
        { // isMember) { // temporarily disabled; allows for self.<variable name> to be used without prefixing with 'self.'
            m_selfMemberAccess.Reset(new AstMember(
                m_name,
                RC<AstVariable>(new AstVariable(
                    "self",
                    m_location)),
                m_location));

            m_selfMemberAccess->Visit(visitor, mod);
        }
        else
        {
            m_isInRefAssignment = mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG);
            m_isInConstAssignment = mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::CONST_VARIABLE_FLAG);

            if (m_isInRefAssignment)
            {
                if (isConst && !m_isInConstAssignment)
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_const_assigned_to_non_const_ref,
                        m_location,
                        m_name));
                }
            }

            if (isGeneric)
            {
                if (!mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_GENERIC_INSTANTIATION))
                { //&& !mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_ALIAS_DECLARATION)
                    //     && !mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG)) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_generic_expression_no_arguments_provided,
                        m_location,
                        m_name));

                    return;
                }
            }

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            const bool forceInline = isAlias || isMixin; // || isSubstitution;

            if (forceInline && m_inlineValue == nullptr)
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_cannot_inline_variable,
                    m_location,
                    m_name));
            }

            // don't inline arguments.
            // can run into an issue with a param is const with default assignment,
            // where it would inline the default assignment instead of the passed in value
            m_shouldInline = forceInline || (!isGeneric && isConst && !isArgument);

            if (m_shouldInline)
            {
                if (m_inlineValue != nullptr)
                {
                    // set access options for this variable based on those of the current value
                    m_accessOptions = m_inlineValue->GetAccessOptions();
                    // if alias, accept the current value instead
                    m_inlineValue->Visit(visitor, mod);
                }
                else
                {
                    m_shouldInline = false;
                }
            }
            else
            {
                m_inlineValue.Reset();
            }
#else
            static constexpr bool forceInline = false;
            m_shouldInline = false;
#endif

            // if it is alias or mixin, update symbol type of this expression

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            // increase usage count for variable loads (non-inlined)
            if (!m_shouldInline)
            {
#endif
                m_properties.GetIdentifier()->IncUseCount();

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            }
#endif

            if (m_properties.IsInFunction())
            {
                const SymbolTypeFlags flags = m_properties.GetIdentifier()->GetFlags();

                // if the variable is declared in a function, and is not a generic substitution,
                // we add it to the closure capture list.
                if ((flags & FLAG_DECLARED_IN_FUNCTION) && !(flags & FLAG_GENERIC_SUBSTITUTION))
                {
                    // lookup the variable by depth to make sure it was declared in the current function
                    if (!mod->LookUpIdentifierDepth(m_name, m_properties.GetDepth()))
                    {
                        Scope* functionScope = m_properties.GetFunctionScope();
                        Assert(functionScope != nullptr);

                        functionScope->AddClosureCapture(
                            m_name,
                            m_properties.GetIdentifier());

                        // closures are objects with a method named '$invoke',
                        // because we are in the '$invoke' method currently,
                        // we use the variable as 'self.<variable name>'
                        m_closureMemberAccess.Reset(new AstMember(
                            m_name,
                            RC<AstVariable>(new AstVariable(
                                "$functor",
                                m_location)),
                            m_location));

                        m_closureMemberAccess->Visit(visitor, mod);
                    }
                }
            }
        }

        break;
    }
    case IDENTIFIER_TYPE_MODULE:
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_identifier_is_module,
            m_location,
            m_name));
        break;
    case IDENTIFIER_TYPE_NOT_FOUND:
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_undeclared_identifier,
            m_location,
            m_name,
            mod->GenerateFullModuleName()));
        break;
    default:
        break;
    }

    if (auto heldType = GetHeldType())
    {
        heldType = heldType->GetUnaliased();

        m_typeRef.Reset(new AstTypeRef(
            heldType,
            m_location));

        m_typeRef->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstVariable::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_isVisited);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_closureMemberAccess != nullptr)
    {
        m_closureMemberAccess->SetAccessMode(m_accessMode);
        return m_closureMemberAccess->Build(visitor, mod);
    }
    else if (m_selfMemberAccess != nullptr)
    {
        m_selfMemberAccess->SetAccessMode(m_accessMode);
        return m_selfMemberAccess->Build(visitor, mod);
    }
    else if (m_typeRef != nullptr)
    {
        m_typeRef->SetAccessMode(m_accessMode);
        return m_typeRef->Build(visitor, mod);
    }

    Assert(m_properties.GetIdentifierType() == IDENTIFIER_TYPE_VARIABLE);
    Assert(m_properties.GetIdentifier() != nullptr, "Identifier not found for variable %s", m_name.Data());

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
    if (m_shouldInline)
    {
        // if alias, accept the current value instead
        const AccessMode currentAccessMode = m_inlineValue->GetAccessMode();
        m_inlineValue->SetAccessMode(m_accessMode);
        chunk->Append(m_inlineValue->Build(visitor, mod));
        // reset access mode
        m_inlineValue->SetAccessMode(currentAccessMode);
    }
    else
    {
#endif
        int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int stackLocation = m_properties.GetIdentifier()->GetStackLocation();

        Assert(stackLocation != -1, "Variable %s has invalid stack location stored; maybe the AstVariableDeclaration was not built?", m_name.Data());

        int offset = stackSize - stackLocation;

        // get active register
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // if we are a reference, we deference it before doing anything
        const bool isRef = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_REF;

        if (m_properties.GetIdentifier()->GetFlags() & FLAG_DECLARED_IN_FUNCTION)
        {
            if (m_accessMode == ACCESS_MODE_LOAD)
            {
                // load stack value at offset value into register
                auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
                instrLoadOffset->GetBuilder().Load(rp, m_isInRefAssignment && !isRef).Local().ByOffset(offset);
                chunk->Append(std::move(instrLoadOffset));

                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                if (isRef && !m_isInRefAssignment)
                {
                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));

                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));
                }
            }
            else if (m_accessMode == ACCESS_MODE_STORE)
            {
                // store the value at (rp - 1) into this local variable
                auto instrMovIndex = BytecodeUtil::Make<StorageOperation>();
                instrMovIndex->GetBuilder().Store(rp - 1).Local().ByOffset(offset);
                chunk->Append(std::move(instrMovIndex));

                chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));
            }
        }
        else
        {
            // load globally, rather than from offset.
            if (m_accessMode == ACCESS_MODE_LOAD)
            {
                // load stack value at index into register
                auto instrLoadIndex = BytecodeUtil::Make<StorageOperation>();
                instrLoadIndex->GetBuilder().Load(rp, m_isInRefAssignment && !isRef).Local().ByIndex(stackLocation);
                chunk->Append(std::move(instrLoadIndex));

                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                if (isRef && !m_isInRefAssignment)
                {
                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));

                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));
                }
            }
            else if (m_accessMode == ACCESS_MODE_STORE)
            {
                // store the value at the index into this local variable
                auto instrMovIndex = BytecodeUtil::Make<StorageOperation>();
                instrMovIndex->GetBuilder().Store(rp - 1).Local().ByIndex(stackLocation);
                chunk->Append(std::move(instrMovIndex));

                chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));
            }
        }

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
    }
#endif

    return chunk;
}

void AstVariable::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->Optimize(visitor, mod);
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->Optimize(visitor, mod);
    }

    if (m_closureMemberAccess != nullptr)
    {
        m_closureMemberAccess->Optimize(visitor, mod);
    }

    if (m_selfMemberAccess != nullptr)
    {
        m_selfMemberAccess->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstVariable::Clone() const
{
    return CloneImpl();
}

Tribool AstVariable::IsTrue() const
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->IsTrue();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->IsTrue();
    }

    // if (m_closureMemberAccess != nullptr) {
    //     return m_closureMemberAccess->IsTrue();
    // }

    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstVariable::MayHaveSideEffects() const
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->MayHaveSideEffects();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->MayHaveSideEffects();
    }

    // if (m_closureMemberAccess != nullptr) {
    //     return m_closureMemberAccess->MayHaveSideEffects();
    // }

    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->MayHaveSideEffects();
    }

    // a simple variable reference does not cause side effects
    return false;
}

bool AstVariable::IsLiteral() const
{
    if (SymbolTypeRef exprType = GetExprType())
    {
        exprType = exprType->GetUnaliased();

        if (exprType->IsObject() || exprType->IsClass())
        {
            return false;
        }

        if (exprType->IsAnyType())
        {
            return false;
        }

        if (!(exprType->IsIntegral() || exprType->IsFloat()))
        {
            return false;
        }
    }
    else
    {
        // undefined
        return false;
    }

    if (m_typeRef != nullptr)
    {
        return m_typeRef->IsLiteral();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->IsLiteral();
    }

    // if (m_closureMemberAccess != nullptr) {
    //     return m_closureMemberAccess->IsLiteral();
    // }

    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->IsLiteral();
    }

    if (const RC<Identifier>& ident = m_properties.GetIdentifier())
    {
        const Identifier* identUnaliased = ident->Unalias();
        Assert(identUnaliased != nullptr);

        const bool isConst = identUnaliased->GetFlags() & IdentifierFlags::FLAG_CONST;
        const bool isGeneric = identUnaliased->GetFlags() & IdentifierFlags::FLAG_GENERIC;
        const bool isArgument = identUnaliased->GetFlags() & IdentifierFlags::FLAG_ARGUMENT;

        return !isArgument && (isConst || isGeneric);
    }

    return false;
}

SymbolTypeRef AstVariable::GetExprType() const
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->GetExprType();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->GetExprType();
    }

    // if (m_closureMemberAccess != nullptr) {
    //     return m_closureMemberAccess->GetExprType();
    // }

    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->GetExprType();
    }

    if (m_properties.GetIdentifier() != nullptr && m_properties.GetIdentifier()->GetSymbolType() != nullptr)
    {
        return m_properties.GetIdentifier()->GetSymbolType();
    }

    return BuiltinTypes::UNDEFINED;
}

bool AstVariable::IsMutable() const
{
    if (IsLiteral())
    {
        return false;
    }

    if (m_typeRef != nullptr)
    {
        return m_typeRef->IsMutable();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->IsMutable();
    }

    // if (m_closureMemberAccess != nullptr) {
    //     return m_closureMemberAccess->IsMutable();
    // }

    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->IsMutable();
    }

    if (const RC<Identifier>& ident = m_properties.GetIdentifier())
    {
        const Identifier* identUnaliased = ident->Unalias();
        Assert(identUnaliased != nullptr);

        const bool isConst = identUnaliased->GetFlags() & IdentifierFlags::FLAG_CONST;

        if (isConst)
        {
            return false;
        }
    }

    return true;
}

const AstExpression* AstVariable::GetValueOf() const
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->GetValueOf();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->GetValueOf();
    }

    return AstIdentifier::GetValueOf();
}

const AstExpression* AstVariable::GetDeepValueOf() const
{
    if (m_typeRef != nullptr)
    {
        return m_typeRef->GetDeepValueOf();
    }

    if (m_inlineValue != nullptr)
    {
        return m_inlineValue->GetDeepValueOf();
    }

    return AstIdentifier::GetDeepValueOf();
}

} // namespace hyperion::compiler
