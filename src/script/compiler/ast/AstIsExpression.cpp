#include <script/compiler/ast/AstIsExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstIsExpression::AstIsExpression(
    const RC<AstExpression> &target,
    const RC<AstPrototypeSpecification> &type_specification,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
    m_type_specification(type_specification),
    m_is_type(TRI_INDETERMINATE)
{
}

void AstIsExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    AssertThrow(m_type_specification != nullptr);
    m_type_specification->Visit(visitor, mod);

    if (const auto target_type = m_target->GetExprType()) {
        if (const auto held_type = m_type_specification->GetHeldType()) {
            if (target_type->TypeCompatible(*held_type, true)) {
                m_is_type = TRI_TRUE;
            } else {
                m_is_type = TRI_FALSE;
            }
        }
    }

    if (m_is_type == TRI_INDETERMINATE) {
        // TODO: Run time check
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstIsExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_is_type == TRI_INDETERMINATE) {
        // TODO: runtime check
    } else {
        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        chunk->Append(BytecodeUtil::Make<ConstBool>(rp, bool(m_is_type.Value())));
    }

    return std::move(chunk);
}

void AstIsExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Optimize(visitor, mod);

    AssertThrow(m_type_specification != nullptr);
    m_type_specification->Optimize(visitor, mod);
}

RC<AstStatement> AstIsExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstIsExpression::GetExprType() const
{
    return BuiltinTypes::BOOLEAN;
}

Tribool AstIsExpression::IsTrue() const
{
    return m_is_type;
}

bool AstIsExpression::MayHaveSideEffects() const
{
    AssertThrow(
        m_target != nullptr && m_type_specification != nullptr
    );

    return m_target->MayHaveSideEffects()
        || m_type_specification->MayHaveSideEffects();
}

} // namespace hyperion::compiler
