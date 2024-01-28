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
#include <system/Debug.hpp>

#include <Types.hpp>

#include <iostream>

namespace hyperion::compiler {

AstVariable::AstVariable(
    const String &name,
    const SourceLocation &location
) : AstIdentifier(name, location),
    m_should_inline(false),
    m_is_in_ref_assignment(false),
    m_is_in_const_assignment(false)
{
}

void AstVariable::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_is_visited);
    m_is_visited = true;

    AstIdentifier::Visit(visitor, mod);

    AssertThrow(m_properties.GetIdentifierType() != IDENTIFIER_TYPE_UNKNOWN);

    switch (m_properties.GetIdentifierType()) {
    case IDENTIFIER_TYPE_VARIABLE: {
        AssertThrow(m_properties.GetIdentifier() != nullptr);

        // if alias or const, load direct value.
        // if it's an alias then it will just refer to whatever other variable
        // is being referenced. if it is const, load the direct value held in the variable
        const bool is_alias = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_ALIAS;
        const bool is_mixin = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_MIXIN;
        const bool is_generic = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_GENERIC;
        const bool is_argument = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_ARGUMENT;
        const bool is_const = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_CONST;
        const bool is_ref = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_REF;
        const bool is_member = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_MEMBER;
        const bool is_substitution = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_GENERIC_SUBSTITUTION;

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
        // clone the AST node so we don't double-visit
        if (auto current_value = m_properties.GetIdentifier()->GetCurrentValue()) {
            const AstConstant *constant_value = nullptr;

            if (current_value->IsLiteral() && (constant_value = dynamic_cast<const AstConstant *>(current_value->GetDeepValueOf()))) {
                m_inline_value = CloneAstNode(constant_value);
            }
        }
#endif

        if (false) {//is_member) { // temporarily disabled; allows for self.<variable name> to be used without prefixing with 'self.'
            m_self_member_access.Reset(new AstMember(
                m_name,
                RC<AstVariable>(new AstVariable(
                    "self",
                    m_location
                )),
                m_location
            ));

            m_self_member_access->Visit(visitor, mod);
        } else {
            m_is_in_ref_assignment = mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG);
            m_is_in_const_assignment = mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::CONST_VARIABLE_FLAG);

            if (m_is_in_ref_assignment) {
                if (is_const && !m_is_in_const_assignment) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_const_assigned_to_non_const_ref,
                        m_location,
                        m_name
                    ));
                }
            }

            if (is_generic) {
                if (!mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_GENERIC_INSTANTIATION)) { //&& !mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_ALIAS_DECLARATION)
                //     && !mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG)) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_generic_expression_no_arguments_provided,
                        m_location,
                        m_name
                    ));

                    return;
                }
            }

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            const bool force_inline = is_alias || is_mixin;// || is_substitution;

            if (force_inline && m_inline_value == nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_cannot_inline_variable,
                    m_location,
                    m_name
                ));
            }

            // don't inline arguments.
            // can run into an issue with a param is const with default assignment,
            // where it would inline the default assignment instead of the passed in value
            m_should_inline = force_inline || (!is_generic && is_const && !is_argument);

            if (m_should_inline) {
                if (m_inline_value != nullptr) {
                    // set access options for this variable based on those of the current value
                    m_access_options = m_inline_value->GetAccessOptions();
                    // if alias, accept the current value instead
                    m_inline_value->Visit(visitor, mod);
                } else {
                    m_should_inline = false;
                }
            } else {
                m_inline_value.Reset();
            }
#else
            static constexpr bool force_inline = false;
            m_should_inline = false;
#endif

            // if it is alias or mixin, update symbol type of this expression


#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            // increase usage count for variable loads (non-inlined)
            if (!m_should_inline) {
#endif
                m_properties.GetIdentifier()->IncUseCount();

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
            }
#endif

            if (m_properties.IsInFunction()) {
                if (m_properties.IsInPureFunction()) {
                    // check if pure function - in a pure function, only variables from this scope may be used
                    if (!mod->LookUpIdentifierDepth(m_name, m_properties.GetDepth())) {
                        // add error that the variable must be passed as a parameter
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_pure_function_scope,
                            m_location,
                            m_name
                        ));
                    }
                }

                const SymbolTypeFlags flags = m_properties.GetIdentifier()->GetFlags();

                // if the variable is declared in a function, and is not a generic substitution,
                // we add it to the closure capture list.
                if ((flags & FLAG_DECLARED_IN_FUNCTION) && !(flags & FLAG_GENERIC_SUBSTITUTION)) {
                    // lookup the variable by depth to make sure it was declared in the current function
                    if (!mod->LookUpIdentifierDepth(m_name, m_properties.GetDepth())) {
                        Scope *function_scope = m_properties.GetFunctionScope();
                        AssertThrow(function_scope != nullptr);

                        function_scope->AddClosureCapture(
                            m_name,
                            m_properties.GetIdentifier()
                        );

                        // closures are objects with a method named '$invoke',
                        // because we are in the '$invoke' method currently,
                        // we use the variable as 'self.<variable name>'
                        m_closure_member_access.Reset(new AstMember(
                            m_name,
                            RC<AstVariable>(new AstVariable(
                                "$functor",
                                m_location
                            )),
                            m_location
                        ));

                        m_closure_member_access->Visit(visitor, mod);
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
            m_name
        ));
        break;
    case IDENTIFIER_TYPE_NOT_FOUND:
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_undeclared_identifier,
            m_location,
            m_name,
            mod->GenerateFullModuleName()
        ));
        break;
    default:
        break;
    }

    if (auto held_type = GetHeldType()) {
        held_type = held_type->GetUnaliased();

        m_type_ref.Reset(new AstTypeRef(
            held_type,
            m_location
        ));

        m_type_ref->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstVariable::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_closure_member_access != nullptr) {
        m_closure_member_access->SetAccessMode(m_access_mode);
        return m_closure_member_access->Build(visitor, mod);
    } else if (m_self_member_access != nullptr) {
        m_self_member_access->SetAccessMode(m_access_mode);
        return m_self_member_access->Build(visitor, mod);
    } else if (m_type_ref != nullptr) {
        m_type_ref->SetAccessMode(m_access_mode);
        return m_type_ref->Build(visitor, mod);
    }
    
    AssertThrow(m_properties.GetIdentifierType() == IDENTIFIER_TYPE_VARIABLE);
    AssertThrowMsg(m_properties.GetIdentifier() != nullptr, "Identifier not found for variable %s", m_name.Data());

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
    if (m_should_inline) {
        // if alias, accept the current value instead
        const AccessMode current_access_mode = m_inline_value->GetAccessMode();
        m_inline_value->SetAccessMode(m_access_mode);
        chunk->Append(m_inline_value->Build(visitor, mod));
        // reset access mode
        m_inline_value->SetAccessMode(current_access_mode);
    } else {
#endif
        Int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        Int stack_location = m_properties.GetIdentifier()->GetStackLocation();

        AssertThrowMsg(stack_location != -1, "Variable %s has invalid stack location stored; maybe the AstVariableDeclaration was not built?", m_name.Data());

        Int offset = stack_size - stack_location;

        // get active register
        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // if we are a reference, we deference it before doing anything
        const bool is_ref = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_REF;

        if (m_properties.GetIdentifier()->GetFlags() & FLAG_DECLARED_IN_FUNCTION) {
            if (m_access_mode == ACCESS_MODE_LOAD) {
                // load stack value at offset value into register
                auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
                instr_load_offset->GetBuilder().Load(rp, m_is_in_ref_assignment && !is_ref).Local().ByOffset(offset);
                chunk->Append(std::move(instr_load_offset));

                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                if (is_ref && !m_is_in_ref_assignment) {
                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));

                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));
                }
            } else if (m_access_mode == ACCESS_MODE_STORE) {
                // store the value at (rp - 1) into this local variable
                auto instr_mov_index = BytecodeUtil::Make<StorageOperation>();
                instr_mov_index->GetBuilder().Store(rp - 1).Local().ByOffset(offset);
                chunk->Append(std::move(instr_mov_index));

                chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));
            }
        } else {
            // load globally, rather than from offset.
            if (m_access_mode == ACCESS_MODE_LOAD) {
                // load stack value at index into register
                auto instr_load_index = BytecodeUtil::Make<StorageOperation>();
                instr_load_index->GetBuilder().Load(rp, m_is_in_ref_assignment && !is_ref).Local().ByIndex(stack_location);
                chunk->Append(std::move(instr_load_index));

                chunk->Append(BytecodeUtil::Make<Comment>("Load variable " + m_name));

                if (is_ref && !m_is_in_ref_assignment) {
                    chunk->Append(BytecodeUtil::Make<LoadDeref>(rp, rp));

                    chunk->Append(BytecodeUtil::Make<Comment>("Dereference variable " + m_name));
                }
            } else if (m_access_mode == ACCESS_MODE_STORE) {
                // store the value at the index into this local variable
                auto instr_mov_index = BytecodeUtil::Make<StorageOperation>();
                instr_mov_index->GetBuilder().Store(rp - 1).Local().ByIndex(stack_location);
                chunk->Append(std::move(instr_mov_index));

                chunk->Append(BytecodeUtil::Make<Comment>("Store variable " + m_name));
            }
        }

#if HYP_SCRIPT_ENABLE_VARIABLE_INLINING
    }
#endif

    return chunk;
}

void AstVariable::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_type_ref != nullptr) {
        return m_type_ref->Optimize(visitor, mod);
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->Optimize(visitor, mod);
    }

    if (m_closure_member_access != nullptr) {
        m_closure_member_access->Optimize(visitor, mod);
    }

    if (m_self_member_access != nullptr) {
        m_self_member_access->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstVariable::Clone() const
{
    return CloneImpl();
}

Tribool AstVariable::IsTrue() const
{
    if (m_type_ref != nullptr) {
        return m_type_ref->IsTrue();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->IsTrue();
    }

    // if (m_closure_member_access != nullptr) {
    //     return m_closure_member_access->IsTrue();
    // }

    if (m_self_member_access != nullptr) {
        return m_self_member_access->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstVariable::MayHaveSideEffects() const
{
    if (m_type_ref != nullptr) {
        return m_type_ref->MayHaveSideEffects();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->MayHaveSideEffects();
    }

    // if (m_closure_member_access != nullptr) {
    //     return m_closure_member_access->MayHaveSideEffects();
    // }

    if (m_self_member_access != nullptr) {
        return m_self_member_access->MayHaveSideEffects();
    }

    // a simple variable reference does not cause side effects
    return false;
}

bool AstVariable::IsLiteral() const
{
    if (SymbolTypePtr_t expr_type = GetExprType()) {
        expr_type = expr_type->GetUnaliased();

        if (expr_type->IsObject() || expr_type->IsClass()) {
            return false;
        }

        if (expr_type->IsAnyType()) {
            return false;
        }

        if (expr_type->IsPlaceholderType()) {
            return false;
        }

        if (!(expr_type->IsIntegral() || expr_type->IsFloat())) {
            return false;
        }
    } else {
        // undefined
        return false;
    }

    if (m_type_ref != nullptr) {
        return m_type_ref->IsLiteral();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->IsLiteral();
    }

    // if (m_closure_member_access != nullptr) {
    //     return m_closure_member_access->IsLiteral();
    // }

    if (m_self_member_access != nullptr) {
        return m_self_member_access->IsLiteral();
    }

    if (const RC<Identifier> &ident = m_properties.GetIdentifier()) {
        const Identifier *ident_unaliased = ident->Unalias();
        AssertThrow(ident_unaliased != nullptr);

        const bool is_const = ident_unaliased->GetFlags() & IdentifierFlags::FLAG_CONST;
        const bool is_generic = ident_unaliased->GetFlags() & IdentifierFlags::FLAG_GENERIC;
        const bool is_argument = ident_unaliased->GetFlags() & IdentifierFlags::FLAG_ARGUMENT;

        return !is_argument && (is_const || is_generic);
    }

    return false;
}

SymbolTypePtr_t AstVariable::GetExprType() const
{
    if (m_type_ref != nullptr) {
        return m_type_ref->GetExprType();
    }
 
    if (m_inline_value != nullptr) {
        return m_inline_value->GetExprType();
    }

    // if (m_closure_member_access != nullptr) {
    //     return m_closure_member_access->GetExprType();
    // }

    if (m_self_member_access != nullptr) {
        return m_self_member_access->GetExprType();
    }

    if (m_properties.GetIdentifier() != nullptr && m_properties.GetIdentifier()->GetSymbolType() != nullptr) {
        return m_properties.GetIdentifier()->GetSymbolType();
    }

    return BuiltinTypes::UNDEFINED;
}

bool AstVariable::IsMutable() const
{
    if (IsLiteral()) {
        return false;
    }

    if (m_type_ref != nullptr) {
        return m_type_ref->IsMutable();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->IsMutable();
    }

    // if (m_closure_member_access != nullptr) {
    //     return m_closure_member_access->IsMutable();
    // }

    if (m_self_member_access != nullptr) {
        return m_self_member_access->IsMutable();
    }

    if (const RC<Identifier> &ident = m_properties.GetIdentifier()) {
        const Identifier *ident_unaliased = ident->Unalias();
        AssertThrow(ident_unaliased != nullptr);

        const bool is_const = ident_unaliased->GetFlags() & IdentifierFlags::FLAG_CONST;

        if (is_const) {
            return false;
        }
    }

    return true;
}

const AstExpression *AstVariable::GetValueOf() const
{
    if (m_type_ref != nullptr) {
        return m_type_ref->GetValueOf();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->GetValueOf();
    }

    return AstIdentifier::GetValueOf();
}

const AstExpression *AstVariable::GetDeepValueOf() const
{
    if (m_type_ref != nullptr) {
        return m_type_ref->GetDeepValueOf();
    }

    if (m_inline_value != nullptr) {
        return m_inline_value->GetDeepValueOf();
    }

    return AstIdentifier::GetDeepValueOf();
}

} // namespace hyperion::compiler
