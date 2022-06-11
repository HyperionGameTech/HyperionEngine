#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/Scope.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

#include <iostream>

namespace hyperion::compiler {

AstVariable::AstVariable(
    const std::string &name,
    const SourceLocation &location)
    : AstIdentifier(name, location),
      m_should_inline(false)
{
}

void AstVariable::Visit(AstVisitor *visitor, Module *mod)
{
    AstIdentifier::Visit(visitor, mod);

    AssertThrow(m_properties.GetIdentifierType() != IDENTIFIER_TYPE_UNKNOWN);

    switch (m_properties.GetIdentifierType()) {
        case IDENTIFIER_TYPE_VARIABLE: {
            AssertThrow(m_properties.GetIdentifier() != nullptr);

#if ACE_ENABLE_VARIABLE_INLINING
            // clone the AST node so we don't double-visit
            if (auto current_value = m_properties.GetIdentifier()->GetCurrentValue()) {
                m_inline_value = current_value;//CloneAstNode(current_value);
            }
#endif

            // if alias or const, load direct value.
            // if it's an alias then it will just refer to whatever other variable
            // is being referenced. if it is const, load the direct value held in the variable
            const bool is_alias = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_ALIAS;
            const bool is_mixin = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_MIXIN;
            const bool is_generic = m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_GENERIC;

            const SymbolTypePtr_t ident_type = m_properties.GetIdentifier()->GetSymbolType();
            AssertThrow(ident_type != nullptr);

            const bool is_const = ident_type->IsConstType() || (m_properties.GetIdentifier()->GetFlags() & IdentifierFlags::FLAG_CONST);

#if ACE_ENABLE_VARIABLE_INLINING
            const bool force_inline = is_alias || is_mixin;

            // NOTE: if we are loading a const and current_value == nullptr, proceed with loading the
            // normal way.
            if (force_inline) {
                AssertThrow(m_inline_value != nullptr);
            }

            m_should_inline = force_inline || (!is_generic && is_const);

            if (m_inline_value == nullptr) {
                m_should_inline = false;
            }

            if (m_should_inline) {
                // set access options for this variable based on those of the current value
                m_access_options = m_inline_value->GetAccessOptions();
                // if alias, accept the current value instead
                m_inline_value->Visit(visitor, mod);
            }
#else
            static constexpr bool force_inline = false;
            m_should_inline = false;
#endif

            // if it is alias or mixin, update symbol type of this expression
            if (force_inline) {
                if (SymbolTypePtr_t inline_value_type = m_inline_value->GetExprType()) {
                    // set symbol type to match alias inline value
                    m_properties.GetIdentifier()->SetSymbolType(inline_value_type);
                }
            } else {

#if ACE_ENABLE_VARIABLE_INLINING
                // if the value to be inlined is not literal - do not inline
                if (m_should_inline && !m_inline_value->IsLiteral()) {
                    m_should_inline = false;
                }

                // increase usage count for variable loads (non-inlined)
                if (!m_should_inline) {
#endif
                    m_properties.GetIdentifier()->IncUseCount();

#if ACE_ENABLE_VARIABLE_INLINING
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

                    // NOTE: if we are in a function, and the variable we are loading is declared in a separate function,
                    // we will show an error message saying that the variable must be passed as a parameter to be captured.
                    // the reason for this is that any variables owned by the parent function will be immediately popped from the stack
                    // when the parent function returns. That will mean the variables used here will reference garbage.
                    // In the near feature, it'd be possible to automatically make a copy of those variables referenced and store them
                    // on the stack of /this/ function.
                    if (m_properties.GetIdentifier()->GetFlags() & FLAG_DECLARED_IN_FUNCTION) {
                        // lookup the variable by depth to make sure it was declared in the current function
                        // we do this to make sure it was declared in this scope.
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
                            m_closure_member_access.reset(new AstMember(
                                m_name,
                                std::shared_ptr<AstVariable>(new AstVariable(
                                    "__closure_self",
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
        case IDENTIFIER_TYPE_TYPE:
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_identifier_is_type,
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
}

std::unique_ptr<Buildable> AstVariable::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_closure_member_access != nullptr) {
        m_closure_member_access->SetAccessMode(m_access_mode);
        return m_closure_member_access->Build(visitor, mod);
    } else {
        AssertThrow(m_properties.GetIdentifier() != nullptr);

#if ACE_ENABLE_VARIABLE_INLINING
        if (m_should_inline) {
            // if alias, accept the current value instead
            const AccessMode current_access_mode = m_inline_value->GetAccessMode();
            m_inline_value->SetAccessMode(m_access_mode);
            chunk->Append(m_inline_value->Build(visitor, mod));
            // reset access mode
            m_inline_value->SetAccessMode(current_access_mode);
        } else {
#endif
            int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            int stack_location = m_properties.GetIdentifier()->GetStackLocation();

            AssertThrow(stack_location != -1);

            int offset = stack_size - stack_location;

            // get active register
            uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            if (m_properties.GetIdentifier()->GetFlags() & FLAG_DECLARED_IN_FUNCTION) {
                if (m_access_mode == ACCESS_MODE_LOAD) {
                    // load stack value at offset value into register
                    auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
                    instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(offset);
                    chunk->Append(std::move(instr_load_offset));
                } else if (m_access_mode == ACCESS_MODE_STORE) {
                    // store the value at (rp - 1) into this local variable
                    auto instr_mov_index = BytecodeUtil::Make<StorageOperation>();
                    instr_mov_index->GetBuilder().Store(rp - 1).Local().ByOffset(offset);
                    chunk->Append(std::move(instr_mov_index));
                }
            } else {
                // load globally, rather than from offset.
                if (m_access_mode == ACCESS_MODE_LOAD) {
                    // load stack value at index into register
                    auto instr_load_index = BytecodeUtil::Make<StorageOperation>();
                    instr_load_index->GetBuilder().Load(rp).Local().ByIndex(stack_location);
                    chunk->Append(std::move(instr_load_index));
                } else if (m_access_mode == ACCESS_MODE_STORE) {
                    // store the value at the index into this local variable
                    auto instr_mov_index = BytecodeUtil::Make<StorageOperation>();
                    instr_mov_index->GetBuilder().Store(rp - 1).Local().ByIndex(stack_location);
                    chunk->Append(std::move(instr_mov_index));
                }
            }

#if ACE_ENABLE_VARIABLE_INLINING
        }
#endif
    }

    return std::move(chunk);
}

void AstVariable::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstVariable::Clone() const
{
    return CloneImpl();
}

Tribool AstVariable::IsTrue() const
{
    if (m_should_inline && m_inline_value != nullptr) {
        if (auto *constant = dynamic_cast<AstConstant*>(m_inline_value.get())) {
            return constant->IsTrue();
        }
    }

    return Tribool::Indeterminate();
}

bool AstVariable::MayHaveSideEffects() const
{
    // a simple variable reference does not cause side effects
    return false;
}

bool AstVariable::IsLiteral() const
{
    const Identifier *ident = m_properties.GetIdentifier();
    AssertThrow(ident != nullptr);

    const Identifier *ident_unaliased = ident->Unalias();
    AssertThrow(ident_unaliased != nullptr);

    const SymbolTypePtr_t ident_type = ident_unaliased->GetSymbolType();
    AssertThrow(ident_type != nullptr);

    const bool is_const = ident_type->IsConstType();
    const bool is_generic = ident_unaliased->GetFlags() & IdentifierFlags::FLAG_GENERIC;

    return is_const || is_generic;
}

SymbolTypePtr_t AstVariable::GetExprType() const
{
    if (m_should_inline) {
        AssertThrow(m_inline_value != nullptr);
        return m_inline_value->GetExprType();
    }

    if (m_properties.GetIdentifier() != nullptr) {
        if (m_properties.GetIdentifier()->GetSymbolType() != nullptr) {
            return m_properties.GetIdentifier()->GetSymbolType();
        }
    }

    return BuiltinTypes::UNDEFINED;
}

} // namespace hyperion::compiler
