#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <iostream>

namespace hyperion {
namespace compiler {

AstVariableDeclaration::AstVariableDeclaration(
    const std::string &name,
    const std::shared_ptr<AstTypeSpecification> &type_specification,
    const std::shared_ptr<AstExpression> &assignment,
    IdentifierFlagBits flags,
    const SourceLocation &location
) : AstDeclaration(name, location),
    m_type_specification(type_specification),
    m_assignment(assignment),
    m_flags(flags),
    m_assignment_already_visited(false)
{
}

void AstVariableDeclaration::Visit(AstVisitor *visitor, Module *mod)
{
    SymbolTypePtr_t symbol_type;

    if (m_type_specification == nullptr && m_assignment == nullptr) {
        // error; requires either type, or assignment.
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_missing_type_and_assignment,
            m_location,
            m_name
        ));
    } else {
        if (m_assignment != nullptr) {
            m_real_assignment = m_assignment;
        }

        // the type_strict flag means that errors will be shown if
        // the assignment type and the user-supplied type differ.
        // it is to be turned off for built-in (default) values
        // for example, the default type of Array is Any.
        // this flag will allow an Array(Float) to be constructed 
        // with an empty array of type Array(Any)
        bool is_type_strict = true;

        if (m_type_specification != nullptr) {
            m_type_specification->Visit(visitor, mod);

            symbol_type = m_type_specification->GetSymbolType();
            AssertThrow(symbol_type != nullptr);

#if ACE_ANY_ONLY_FUNCTION_PARAMATERS
            if (symbol_type == BuiltinTypes::ANY) {
                // Any type is reserved for method parameters
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_any_reserved_for_parameters,
                    m_location
                ));
            }
#endif

            // if no assignment provided, set the assignment to be the default value of the provided type
            if (m_real_assignment == nullptr) {
                // generic/non-concrete types that have default values
                // will get assigned to their default value without causing
                // an error
                if (symbol_type->GetDefaultValue() != nullptr) {
                    // Assign variable to the default value for the specified type.
                    m_real_assignment = symbol_type->GetDefaultValue();
                    // built-in assignment, turn off strict mode
                    is_type_strict = false;
                } else if (symbol_type->GetTypeClass() == TYPE_GENERIC) {
                    // generic not yet promoted to an instance.
                    // since there is no assignment to go by, we can't promote it

                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_generic_parameters_missing,
                        m_location,
                        symbol_type->GetName(),
                        symbol_type->GetGenericInfo().m_num_parameters
                    ));
                } else {
                    // generic parameters will be resolved upon instantiation
                    if (!symbol_type->IsGenericParameter()) {
                        // no default assignment for this type
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_type_no_default_assignment,
                            m_location,
                            symbol_type->GetName()
                        ));
                    }
                }
            }
        }

        if (m_real_assignment == nullptr) {
            m_real_assignment.reset(new AstUndefined(m_location));
        }

        if (!m_assignment_already_visited) {
            // visit assignment
            m_real_assignment->Visit(visitor, mod);
        }

        if (m_assignment != nullptr) {
            // has received an explicit assignment
            // make sure type is compatible with assignment
            SymbolTypePtr_t assignment_type = m_real_assignment->GetSymbolType();
            AssertThrow(assignment_type != nullptr);

            if (m_type_specification != nullptr) {
                // symbol_type should be the user-specified type
                symbol_type = SymbolType::GenericPromotion(symbol_type, assignment_type);
                AssertThrow(symbol_type != nullptr);

                // generic not yet promoted to an instance
                if (symbol_type->GetTypeClass() == TYPE_GENERIC) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_generic_parameters_missing,
                        m_location,
                        symbol_type->GetName(),
                        symbol_type->GetGenericInfo().m_num_parameters
                    ));
                }

                if (is_type_strict) {
                    SymbolTypePtr_t comparison_type = symbol_type;

                    // unboxing values
                    // note that this is below the default assignment check,
                    // because these "box" types may have a default assignment of their own
                    // (or they may intentionally not have one)
                    // e.g Maybe(T) defaults to null, and Const(T) has no assignment.
                    if (symbol_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
                        if (symbol_type->IsBoxedType()) {
                            comparison_type = symbol_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
                            AssertThrow(comparison_type != nullptr);
                        }
                    }

                    SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
                        visitor,
                        mod,
                        comparison_type,
                        assignment_type,
                        m_real_assignment->GetLocation()
                    );
                }
            } else {
                // Set the type to be the deduced type from the expression.
                symbol_type = assignment_type;
            }
        }
    }

    AstDeclaration::Visit(visitor, mod);

    if (m_identifier != nullptr) {
        if (IsConst()) {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_CONST);
        }

        m_identifier->SetSymbolType(symbol_type);
        m_identifier->SetCurrentValue(m_real_assignment);
    }
}

std::unique_ptr<Buildable> AstVariableDeclaration::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_real_assignment != nullptr);

    if (!hyperion::compiler::Config::cull_unused_objects || m_identifier->GetUseCount() > 0) {
        // get current stack size
        const int stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // set identifier stack location
        m_identifier->SetStackLocation(stack_location);

        chunk->Append(m_real_assignment->Build(visitor, mod));

        // get active register
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // add instruction to store on stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<uint8_t>(rp);
            chunk->Append(std::move(instr_push));
        }

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    } else {
        // if assignment has side effects but variable is unused,
        // compile the assignment in anyway.
        if (m_real_assignment->MayHaveSideEffects()) {
            chunk->Append(m_real_assignment->Build(visitor, mod));
        }
    }

    return std::move(chunk);
}

void AstVariableDeclaration::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_real_assignment != nullptr) {
        m_real_assignment->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstVariableDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
