#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <Core/debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <iostream>

namespace Hyperion {

AstVariableDeclaration::AstVariableDeclaration(
    const String& name,
    const RC<AstTypeSpecifier>& typeSpec,
    const RC<AstExpression>& assignment,
    EnumFlags<IdentifierFlags> flags,
    const SourceLocation& location)
    : AstDeclaration(name, flags, location),
      m_typeSpec(typeSpec),
      m_assignment(assignment),
      m_symbolType(nullptr)
{
}

void AstVariableDeclaration::Visit(AstVisitor* visitor, Module* mod)
{
    if (m_flags & IdentifierFlags::PREREGISTER)
    {
        m_symbolType = BuiltinTypes::s_anyType;

        AstDeclaration::Visit(visitor, mod);

        if (m_identifier != nullptr)
        {
            m_identifier->GetFlags() |= m_flags;
            m_identifier->SetSymbolType(m_symbolType);
            m_identifier->SetCurrentValue(m_realAssignment);
        }

        // if pre-registering, do not visit assignment
        return;
    }

    m_symbolType = BuiltinTypes::s_errorType;

    const bool hasUserSpecifiedType = m_typeSpec != nullptr;
    const bool hasUserAssigned = m_assignment != nullptr;

    bool isDefaultAssigned = false;

    if (hasUserAssigned)
    {
        m_realAssignment = m_assignment;
    }

    if (!hasUserSpecifiedType && !hasUserAssigned)
    {
        // error; requires either type, or assignment.
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_missing_type_and_assignment,
            m_location,
            m_name));
    }
    else
    {

        // flag that is turned on when there is no default type or assignment (an error)
        bool noDefaultAssignment = false;

        /* ===== handle type specification ===== */
        if (hasUserSpecifiedType)
        {
            m_typeSpec->Visit(visitor, mod);
            m_symbolType = m_typeSpec->GetHeldType();

            // if no assignment provided, set the assignment to be the default value of the provided type
            if (m_realAssignment == nullptr)
            {
                // generic/non-concrete types that have default values
                // will get assigned to their default value without causing
                // an error
                if (const RC<AstExpression>& defaultValue = m_symbolType->GetDefaultValue())
                {
                    // Assign variable to the default value for the specified type.
                    m_realAssignment = CloneAstNode(defaultValue);
                    isDefaultAssigned = true;
                }
                else if (!m_symbolType->IsGenericParameter())
                {
                    // no default assignment for this type
                    noDefaultAssignment = true;
                }
            }
        }

        if (m_realAssignment == nullptr)
        {
            // no assignment found - set to undefined (instead of a null pointer)
            m_realAssignment.Reset(new AstUndefined(m_location));
        }

        // do this only within the scope of the assignment being visited.
        bool passByRefScope = false;
        bool passByConstScope = false;

        if (IsConst())
        {
            mod->scopeTree.Open(SCOPE_TYPE_NORMAL, CONST_VARIABLE_FLAG);

            passByConstScope = true;
        }

        if (IsRef())
        {
            if (hasUserAssigned)
            {
                if (m_realAssignment->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE)
                {
                    mod->scopeTree.Open(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG);

                    passByRefScope = true;
                }
                else
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_cannot_create_reference,
                        m_location));
                }
            }
            else
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_ref_missing_assignment,
                    m_location,
                    m_name));
            }
        }

        // visit assignment
        if (m_name == "$construct")
        { // m_flags & FLAG_CONSTRUCTOR) {
            m_realAssignment->ApplyExpressionFlags(EXPR_FLAGS_CONSTRUCTOR_DEFINITION);
        }

        m_realAssignment->Visit(visitor, mod);

        /* ===== handle assignment ===== */
        // has received an explicit assignment
        // make sure type is compatible with the type.
        if (hasUserAssigned)
        {
            Assert(m_realAssignment->GetExprType() != nullptr);

            if (hasUserSpecifiedType)
            {
                m_symbolType = SemanticAnalyzer::Helpers::GenericPromotion(
                    visitor,
                    mod,
                    m_symbolType,
                    m_realAssignment->GetExprType(),
                    m_realAssignment->GetLocation());

                m_symbolType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
                    visitor,
                    mod,
                    m_symbolType,
                    m_location);

                Assert(m_symbolType != nullptr);
                m_symbolType->Register(visitor->GetCompilationUnit());

                if (!m_realAssignment->GetExprType()->TypeEqual(*m_symbolType))
                {
                    // Allow literals to be promoted to the specified type without as strict checking,
                    // as long as we can prove that the literal can fit in the specified type.
                    bool doLiteralConversion = false;

                    const ConstantValue rightConstantValue = m_realAssignment->GetConstantValue();

                    const SymbolType* lhsType = nullptr;
                    const SymbolType* rhsType = nullptr;

                    if (rightConstantValue.IsValid())
                    {
                        lhsType = m_symbolType->GetUnaliased();
                        rhsType = m_realAssignment->GetExprType();

                        if (lhsType->IsEnumType())
                        {
                            lhsType = lhsType->GetBaseType();
                        }

                        Assert(lhsType != nullptr && rhsType != nullptr);

                        lhsType = lhsType->GetUnaliased();
                        rhsType = rhsType->GetUnaliased();

                        if (lhsType->IsSignedIntegral())
                        {
                            if (rhsType->IsSignedIntegral())
                            {
                                doLiteralConversion |= rightConstantValue.AsInt() >= CBS_Min_Signed(lhsType->GetConstantBitSize())
                                    && rightConstantValue.AsInt() <= CBS_Max_Signed(lhsType->GetConstantBitSize());
                            }
                            else if (rhsType->IsUnsignedIntegral())
                            {
                                doLiteralConversion |= rightConstantValue.AsUInt() <= uint64(CBS_Max_Signed(lhsType->GetConstantBitSize()));
                            }
                        }
                        else if (lhsType->IsUnsignedIntegral())
                        {
                            if (rhsType->IsUnsignedIntegral())
                            {
                                doLiteralConversion |= rightConstantValue.AsUInt() <= uint64(CBS_Max_Unsigned(lhsType->GetConstantBitSize()));
                            }
                            else if (rhsType->IsSignedIntegral())
                            {
                                doLiteralConversion |= rightConstantValue.AsInt() >= 0
                                    && uint64(rightConstantValue.AsInt()) <= uint64(CBS_Max_Unsigned(lhsType->GetConstantBitSize()));
                            }
                        }
                    }

                    // more fine-grained checking for non-literals, or when literal conversion didn't work
                    if (!doLiteralConversion)
                    {
                        SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
                            visitor,
                            mod,
                            m_symbolType,
                            m_realAssignment->GetExprType(),
                            /* strictEnum */ false,
                            /* strictNull */ !(m_flags & IdentifierFlags::LAX),
                            m_realAssignment->GetLocation());
                    }

                    // insert cast if needed
                    if ((doLiteralConversion || !m_realAssignment->GetExprType()->TypeEqual(*m_symbolType)) && !(m_flags & IdentifierFlags::LAX))
                    {
                        RC<AstAsExpression> asExpr(new AstAsExpression(
                            CloneAstNode(m_realAssignment),
                            m_typeSpec,
                            m_realAssignment->GetLocation()));

                        asExpr->Visit(visitor, mod);

                        m_realAssignment = asExpr;
                    }
                }
            }
            else
            {
                m_symbolType = m_realAssignment->GetExprType();
            }
        }

        if (noDefaultAssignment && !(m_flags & (IdentifierFlags::LAX | IdentifierFlags::EXTERN)))
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_type_no_default_assignment,
                m_typeSpec != nullptr
                    ? m_typeSpec->GetLocation()
                    : m_location,
                m_symbolType->ToString()));
        }

        if (passByRefScope)
        {
            mod->scopeTree.Close();
        }

        if (passByConstScope)
        {
            mod->scopeTree.Close();
        }
    }

    if (IsConst())
    {
        if (!hasUserAssigned && !isDefaultAssigned)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_const_missing_assignment,
                m_location,
                m_name));
        }
    }

    if (m_symbolType == nullptr)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_deduce_type_for_expression,
            m_location,
            m_name));

        return;
    }

    AstDeclaration::Visit(visitor, mod);

    if (m_identifier != nullptr)
    {
        m_identifier->GetFlags() |= m_flags;
        m_identifier->SetSymbolType(m_symbolType);
        m_identifier->SetCurrentValue(m_realAssignment);
    }
}

UniquePtr<Buildable> AstVariableDeclaration::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_realAssignment != nullptr);

    if (!ScriptConfig::CullUnusedObjects
        || m_identifier->GetUseCount() > 0
        || (m_flags & (IdentifierFlags::EXTERN | IdentifierFlags::PLACEHOLDER)))
    {
        // update identifier stack location to be current stack size.
        m_identifier->SetStackLocation(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize());

        if (!(m_flags & IdentifierFlags::EXTERN))
        {
            // if the type specification has side effects, compile it in
            if (m_typeSpec != nullptr && m_typeSpec->MayHaveSideEffects())
            {
                chunk->Append(m_typeSpec->Build(visitor, mod));
            }

            chunk->Append(m_realAssignment->Build(visitor, mod));

            // get active register
            uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            { // add instruction to store on stack
                auto instrPush = BytecodeUtil::Make<RawOperation<>>();
                instrPush->opcode = PUSH;
                instrPush->Accept<uint8>(rp);
                chunk->Append(std::move(instrPush));
            }

            // add a comment for debugging to know where the var exists
            chunk->Append(BytecodeUtil::Make<Comment>(" Var `" + m_name + "` at stack location: "
                + String::ToString(m_identifier->GetStackLocation())));
        }
        else
        {
            chunk->Append(BytecodeUtil::Make<Comment>(" Placeholder `" + m_name + "` will be replaced at runtime"));
            { // add instruction increase stack pointer by 1
                auto instrAddSp = BytecodeUtil::Make<RawOperation<>>();
                instrAddSp->opcode = ADD_SP;
                instrAddSp->Accept<uint16>(1);
                chunk->Append(std::move(instrAddSp));
            }
        }

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }
    else
    {
        // if assignment has side effects but variable is unused,
        // compile the assignment in anyway.
        if (m_realAssignment->MayHaveSideEffects())
        {
            chunk->Append(m_realAssignment->Build(visitor, mod));
        }
    }

    return chunk;
}

void AstVariableDeclaration::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_realAssignment != nullptr)
    {
        m_realAssignment->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstVariableDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
