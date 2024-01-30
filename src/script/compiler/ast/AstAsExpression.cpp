#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstAsExpression::AstAsExpression(
    const RC<AstExpression> &target,
    const RC<AstPrototypeSpecification> &type_specification,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
    m_type_specification(type_specification),
    m_is_type(TRI_INDETERMINATE)
{
}

void AstAsExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    AssertThrow(m_type_specification != nullptr);
    m_type_specification->Visit(visitor, mod);

    auto *target_value_of = m_target->GetDeepValueOf();
    AssertThrow(target_value_of != nullptr);

    SymbolTypePtr_t target_type = target_value_of->GetExprType();
    if (target_type == nullptr) {
        return; // should be caught by the type specification
    }

    target_type = target_type->GetUnaliased();

    auto *type_specification_value_of = m_type_specification->GetDeepValueOf();
    AssertThrow(type_specification_value_of != nullptr);

    SymbolTypePtr_t held_type = type_specification_value_of->GetHeldType();
    if (held_type == nullptr) {
        return; // should be caught by the type specification
    }
    
    held_type = held_type->GetUnaliased();

    if (held_type->IsAnyType()) {
        m_is_type = TRI_TRUE;

        return;
    }

    if (held_type->IsPlaceholderType()) {
        m_is_type = TRI_INDETERMINATE;

        return;
    }

    if (target_type->TypeEqual(*held_type)) {
        m_is_type = TRI_TRUE;

        return;
    }

    if (!target_type->TypeCompatible(*held_type, false)) {
        // not compatible
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_incompatible_cast,
            m_location,
            target_type->ToString(),
            held_type->ToString()
        ));

        return;
    }
}

std::unique_ptr<Buildable> AstAsExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_type_specification != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    bool type_spec_built = false;

    // if the type spec has side effects, build it in even though it's not needed for the cast
    if (m_type_specification->MayHaveSideEffects()) {
        chunk->Append(m_type_specification->Build(visitor, mod));

        type_spec_built = true;
    }

    if (m_is_type == TRI_TRUE) {
        // just build the target
        chunk->Append(m_target->Build(visitor, mod));

        return chunk;
    }

    const UInt8 src_register = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // Load the target into src
    chunk->Append(m_target->Build(visitor, mod));

    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    const UInt8 dst_register = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto *value_of = m_type_specification->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

    SymbolTypePtr_t held_type = value_of->GetHeldType();
    AssertThrow(held_type != nullptr);
    held_type = held_type->GetUnaliased();

    AssertThrow(!held_type->IsAnyType());

    if (held_type->IsSignedIntegral()) {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_I32, dst_register, src_register));
    } else if (held_type->IsUnsignedIntegral()) {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_U32, dst_register, src_register));
    } else if (held_type->IsFloat()) {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_F32, dst_register, src_register));
    } else if (held_type->IsBoolean()) {
        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_BOOL, dst_register, src_register));
    } else {
        // dynamic type needs to load the type object into the dst register.
        // if the type spec has side effects, it's already built in
        if (!type_spec_built) {
            chunk->Append(m_type_specification->Build(visitor, mod));

            type_spec_built = true;
        }

        chunk->Append(BytecodeUtil::Make<CastOperation>(CastOperation::CAST_DYNAMIC, dst_register, src_register));
    }

    { // swap dst and src
        auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
        instr_mov_reg->opcode = MOV_REG;
        instr_mov_reg->Accept<UInt8>(src_register);
        instr_mov_reg->Accept<UInt8>(dst_register);
        chunk->Append(std::move(instr_mov_reg));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    return chunk;
}

void AstAsExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Optimize(visitor, mod);

    AssertThrow(m_type_specification != nullptr);
    m_type_specification->Optimize(visitor, mod);
}

RC<AstStatement> AstAsExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstAsExpression::GetExprType() const
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_type_specification != nullptr);

    if (SymbolTypePtr_t held_type = m_type_specification->GetHeldType()) {
        return held_type;
    }

    return BuiltinTypes::UNDEFINED;
}

Tribool AstAsExpression::IsTrue() const
{
    return TRI_INDETERMINATE;
}

bool AstAsExpression::MayHaveSideEffects() const
{
    AssertThrow(
        m_target != nullptr && m_type_specification != nullptr
    );

    return m_target->MayHaveSideEffects()
        || m_type_specification->MayHaveSideEffects();
}

} // namespace hyperion::compiler
