#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstEnumExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion {

AstVariableDeclaration::AstVariableDeclaration(
    const String& name,
    const RC<AstTypeSpecifier>& proto,
    const RC<AstExpression>& assignment,
    IdentifierFlagBits flags,
    const SourceLocation& location)
    : AstDeclaration(name, location),
      m_proto(proto),
      m_assignment(assignment),
      m_flags(flags)
{
}

void AstVariableDeclaration::Visit(AstVisitor* visitor, Module* mod)
{
    m_symbolType = BuiltinTypes::g_errorType;

    const bool hasUserAssigned = m_assignment != nullptr;
    const bool hasUserSpecifiedType = m_proto != nullptr;

    bool isDefaultAssigned = false;

    if (hasUserAssigned)
    {
        m_realAssignment = m_assignment;
    }

    if (m_proto != nullptr)
    {
        m_proto->Visit(visitor, mod);
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
            auto* valueOf = m_proto->GetDeepValueOf();
            Assert(valueOf != nullptr);

            SymbolTypeRef protoExprType = valueOf->GetExprType();
            Assert(protoExprType != nullptr);
            protoExprType = protoExprType->GetUnaliased();

            SymbolTypeRef protoHeldType = valueOf->GetHeldType();
            if (protoHeldType != nullptr)
            {
                protoHeldType = protoHeldType->GetUnaliased();
            }

            if (protoHeldType == nullptr)
            {
                // Add error that invalid type was specified.

                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_not_a_type,
                    m_proto->GetLocation(),
                    protoExprType->ToString()));

                m_symbolType = BuiltinTypes::g_errorType;
            }
            else
            {
                m_symbolType = protoHeldType;
            }

#if HYP_SCRIPT_ANY_ONLY_FUNCTION_PARAMATERS
            if (m_symbolType->IsAnyType())
            {
                // Any type is reserved for method parameters
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_any_reserved_for_parameters,
                    m_location));
            }
#endif

            // if no assignment provided, set the assignment to be the default value of the provided type
            if (m_realAssignment == nullptr)
            {
                // generic/non-concrete types that have default values
                // will get assigned to their default value without causing
                // an error
                if (const RC<AstExpression> defaultValue = m_proto->GetDefaultValue())
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

        // // Set identifier current value in advance,
        // // so from type declarations we can use the current type.
        // // Note: m_identifier will only be nullptr if declaration has failed.
        // if (m_identifier != nullptr) {
        //     auto *expressionValueOf = m_realAssignment->GetDeepValueOf();
        //     Assert(expressionValueOf != nullptr);

        //     m_identifier->SetCurrentValue(CloneAstNode(expressionValueOf));
        // }

        // if the variable has been assigned to an anonymous type,
        // rename the type to be the name of this variable
        if (AstClass* asTypeExpr = dynamic_cast<AstClass*>(m_realAssignment.Get()))
        {
            asTypeExpr->SetName(m_name);
        }
        else if (AstEnumExpression* asEnumExpr = dynamic_cast<AstEnumExpression*>(m_realAssignment.Get()))
        {
            asEnumExpr->SetName(m_name);
        } // @TODO more polymorphic way of doing this..

        // do this only within the scope of the assignment being visited.
        bool passByRefScope = false;
        bool passByConstScope = false;

        if (IsConst())
        {
            mod->m_scopes.Open(SCOPE_TYPE_NORMAL, CONST_VARIABLE_FLAG);

            passByConstScope = true;
        }

        if (IsRef())
        {
            if (hasUserAssigned)
            {
                if (m_realAssignment->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE)
                {
                    mod->m_scopes.Open(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG);

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
                SymbolTypeRef symbolTypePromoted = SemanticAnalyzer::Helpers::GenericPromotion(
                    visitor,
                    mod,
                    m_symbolType,
                    m_realAssignment->GetExprType(),
                    m_realAssignment->GetLocation());

                Assert(symbolTypePromoted != nullptr);
                m_symbolType = symbolTypePromoted->GetUnaliased();

                SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
                    visitor,
                    mod,
                    m_symbolType,
                    m_realAssignment->GetExprType(),
                    m_realAssignment->GetLocation());
            }
            else
            {
                m_symbolType = m_realAssignment->GetExprType();
            }
        }

        if (noDefaultAssignment && !(m_flags & (IdentifierFlags::FLAG_CLOSURE_PLACEHOLDER | IdentifierFlags::FLAG_EXTERN)))
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_type_no_default_assignment,
                m_proto != nullptr
                    ? m_proto->GetLocation()
                    : m_location,
                m_symbolType->ToString()));
        }

        if (passByRefScope)
        {
            mod->m_scopes.Close();
        }

        if (passByConstScope)
        {
            mod->m_scopes.Close();
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

        // set current value to be the assignment
        if (!m_identifier->GetCurrentValue())
        {
            // Note: we do not call CloneAstNode() on the assignment,
            // because we need to use GetExprType(), which requires that the node has been visited.
            m_identifier->SetCurrentValue(m_realAssignment);
        }
    }
}

UniquePtr<Buildable> AstVariableDeclaration::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_realAssignment != nullptr);

    if (!Config::cullUnusedObjects || m_identifier->GetUseCount() > 0 || (m_flags & (IdentifierFlags::FLAG_EXTERN | IdentifierFlags::FLAG_CLOSURE_PLACEHOLDER)))
    {
        // update identifier stack location to be current stack size.
        m_identifier->SetStackLocation(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize());

        if (!(m_flags & IdentifierFlags::FLAG_EXTERN))
        {
            // if the type specification has side effects, compile it in
            if (m_proto != nullptr && m_proto->MayHaveSideEffects())
            {
                chunk->Append(m_proto->Build(visitor, mod));
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

} // namespace hyperion
