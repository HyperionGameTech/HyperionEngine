#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Scope.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <Core/Types.hpp>

#include <iostream>

#include <AstVariable.generated.inl>

namespace Hyperion {

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
        const bool isAlias = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::ALIAS;
        const bool isArgument = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::ARGUMENT;
        const bool isConst = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::CONSTANT;
        const bool isRef = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::REF;

        const bool isMember = (m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::MEMBER)
            && !(m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::STATIC_MEMBER);

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
#error "Variable inlining is no longer supported. Remove HYP_SCRIPT_ENABLE_VARIABLE_INLINING related code."
#endif

        if (isMember) // add 'self' prefix for member access
        {
            m_selfMemberAccess.Reset(new AstMember(
                m_name,
                RC<AstVariable>(new AstVariable("self", m_location)),
                m_location));

            m_selfMemberAccess->Visit(visitor, mod);
        }
        else
        {
            m_isInRefAssignment = mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG, /* thisScopeOnly */ false);
            m_isInConstAssignment = mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, CONST_VARIABLE_FLAG, /* thisScopeOnly */ false);

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

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            const bool forceInline = isAlias;

            // don't inline arguments.
            // can run into an issue with a param is const with default assignment,
            // where it would inline the default assignment instead of the passed in value
            m_shouldInline = forceInline || (isConst && !isArgument);

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
                const EnumFlags<IdentifierFlags> identifierFlags = m_properties.GetIdentifier()->GetFlags();

                // if the variable is declared in a function, and is not a generic substitution,
                // we add it to the closure capture list.
                if (identifierFlags[IdentifierFlags::DECLARED_IN_FUNCTION])
                {
                    // lookup the variable by depth to make sure it was declared in the current function
                    if (!mod->LookUpIdentifierDepth(m_name, m_properties.GetDepth()))
                    {
                        Scope* functionScope = m_properties.GetFunctionScope();
                        Assert(functionScope != nullptr);

                        functionScope->AddClosureCapture(m_name, m_properties.GetIdentifier());

                        // closures are objects with a method named '$invoke',
                        // because we are in the '$invoke' method currently,
                        // we use the variable as 'self.<variable name>'
                        m_closureMemberAccess.Reset(new AstMember(
                            m_name,
                            RC<AstVariable>(new AstVariable("$functor", m_location)),
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

    if (const SymbolType* heldType = GetHeldType())
    {
        heldType = heldType->GetUnaliased();

        m_typeRef.Reset(new AstTypeRef(heldType, m_location));
        m_typeRef->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstVariable::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Variable access: " + m_name + " (access mode: " + String::ToString(static_cast<int>(m_accessMode)) + ")"));

    if (m_closureMemberAccess != nullptr)
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Accessing closure member: " + m_name));
        m_closureMemberAccess->SetAccessMode(m_accessMode);
        return m_closureMemberAccess->Build(visitor, mod);
    }
    else if (m_selfMemberAccess != nullptr)
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Accessing self member: " + m_name));
        m_selfMemberAccess->SetAccessMode(m_accessMode);
        return m_selfMemberAccess->Build(visitor, mod);
    }
    else if (m_typeRef != nullptr)
    {
        const SymbolType* heldType = m_typeRef->GetHeldType();
        Assert(heldType != nullptr);

        chunk->Append(BytecodeUtil::Make<Comment>("Accessing type reference: " + m_name));

        m_typeRef->SetAccessMode(m_accessMode);
        chunk->Append(m_typeRef->Build(visitor, mod));

        return chunk;
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
        const int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        const int stackLocation = m_properties.GetIdentifier()->GetStackLocation();

        Assert(stackLocation != -1, "Variable %s has invalid stack location stored; maybe the AstVariableDeclaration was not built?", m_name.Data());

        const int offset = stackSize - stackLocation;

        // get active register
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // if we are a reference, we deference it before doing anything
        const bool isRef = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::REF;

        if (m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::DECLARED_IN_FUNCTION)
        {
            if (m_accessMode == ACCESS_MODE_LOAD)
            {
                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                // load stack value at offset value into register
                auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
                instrLoadOffset->GetBuilder().Load(rp, m_isInRefAssignment && !isRef).Local().ByOffset(offset);
                chunk->Append(std::move(instrLoadOffset));

                if (isRef && !m_isInRefAssignment)
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));

                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));
                }
            }
            else if (m_accessMode == ACCESS_MODE_STORE)
            {
                if (isRef)
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Store to ref variable " + m_name));

                    // Load the ref variable normally (its stack slot already holds a Reference,
                    // and ShallowCopy returns references as-is), then store through it.
                    auto instrLoad = BytecodeUtil::Make<StorageOperation>();
                    instrLoad->GetBuilder().Load(rp, false).Local().ByOffset(offset);
                    chunk->Append(std::move(instrLoad));

                    // Store the RHS value through the reference using MOV_UNIFIED with deref-dst flag
                    constexpr uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_REGISTER) | MOV_DEREFDST_FLAG;

                    auto instrStoreDeref = BytecodeUtil::Make<RawOperation<>>();
                    instrStoreDeref->opcode = MOV_UNIFIED;
                    instrStoreDeref->Accept<uint8>(subcmd);
                    instrStoreDeref->Accept<uint8>(rp);     // dstRefReg (dereferenced as target)
                    instrStoreDeref->Accept<uint8>(rp - 1); // srcReg (value to store)
                    chunk->Append(std::move(instrStoreDeref));
                }
                else
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));

                    // store the value at (rp - 1) into this local variable
                    auto instrMovIndex = BytecodeUtil::Make<StorageOperation>();
                    instrMovIndex->GetBuilder().Store(rp - 1).Local().ByOffset(offset);
                    chunk->Append(std::move(instrMovIndex));
                }
            }
        }
        else
        {
            // load globally, rather than from offset.
            if (m_accessMode == ACCESS_MODE_LOAD)
            {
                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                // load stack value at index into register
                auto instrLoadIndex = BytecodeUtil::Make<StorageOperation>();
                instrLoadIndex->GetBuilder().Load(rp, m_isInRefAssignment && !isRef).Local().ByIndex(stackLocation);
                chunk->Append(std::move(instrLoadIndex));

                if (isRef && !m_isInRefAssignment)
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));

                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));
                }
            }
            else if (m_accessMode == ACCESS_MODE_STORE)
            {
                if (isRef)
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Store to ref variable " + m_name));

                    // Load the ref variable normally (its stack slot already holds a Reference,
                    // and ShallowCopy returns references as-is), then store through it.
                    auto instrLoad = BytecodeUtil::Make<StorageOperation>();
                    instrLoad->GetBuilder().Load(rp, false).Local().ByIndex(stackLocation);
                    chunk->Append(std::move(instrLoad));

                    // Store the RHS value through the reference using MOV_UNIFIED with deref-dst flag
                    constexpr uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_REGISTER) | MOV_DEREFDST_FLAG;

                    auto instrStoreDeref = BytecodeUtil::Make<RawOperation<>>();
                    instrStoreDeref->opcode = MOV_UNIFIED;
                    instrStoreDeref->Accept<uint8>(subcmd);
                    instrStoreDeref->Accept<uint8>(rp);     // dstRefReg (dereferenced as target)
                    instrStoreDeref->Accept<uint8>(rp - 1); // srcReg (value to store)
                    chunk->Append(std::move(instrStoreDeref));
                }
                else
                {
                    chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));

                    // store the value at the index into this local variable
                    auto instrMovIndex = BytecodeUtil::Make<StorageOperation>();
                    instrMovIndex->GetBuilder().Store(rp - 1).Local().ByIndex(stackLocation);
                    chunk->Append(std::move(instrMovIndex));
                }
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
    return GetConstantValue().IsValid();
}

const SymbolType* AstVariable::GetExprType() const
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

    return BuiltinTypes::s_errorType;
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

        const bool isConst = identUnaliased->GetFlags() & IdentifierFlags::CONSTANT;

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

AstExpression* AstVariable::GetTarget() const
{
    if (m_selfMemberAccess != nullptr)
    {
        return m_selfMemberAccess->GetTarget();
    }

    return AstExpression::GetTarget();
}

} // namespace Hyperion
