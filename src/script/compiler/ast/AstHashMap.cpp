#include <script/compiler/ast/AstHashMap.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <script/Hasher.hpp>
#include <system/Debug.hpp>

#include <core/lib/FlatSet.hpp>

#include <Types.hpp>

namespace hyperion::compiler {

AstHashMap::AstHashMap(
    const Array<RC<AstExpression>> &keys,
    const Array<RC<AstExpression>> &values,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_keys(keys),
    m_values(values)
{
}

void AstHashMap::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_keys.Size() == m_values.Size());

    m_replaced_keys.Reserve(m_keys.Size());
    m_replaced_values.Reserve(m_values.Size());

    m_expr_type = BuiltinTypes::UNDEFINED;

    m_key_type = BuiltinTypes::UNDEFINED;
    m_value_type = BuiltinTypes::UNDEFINED;

    Array<Pair<RC<AstExpression>, RC<AstExpression>>> key_value_pairs;
    key_value_pairs.Reserve(m_keys.Size());

    for (SizeType i = 0; i < m_keys.Size(); ++i) {
        AssertThrow(m_keys[i] != nullptr);
        AssertThrow(m_values[i] != nullptr);

        key_value_pairs.PushBack({ m_keys[i], m_values[i] });
    }

    if (key_value_pairs.Any()) {
        for (auto &key_value_pair : key_value_pairs) {
            key_value_pair.first->Visit(visitor, mod);
            key_value_pair.second->Visit(visitor, mod);

            SymbolTypePtr_t key_type = key_value_pair.first->GetExprType();
            SymbolTypePtr_t value_type = key_value_pair.second->GetExprType();

            if (key_type == nullptr || value_type == nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_internal_error,
                    m_location
                ));

                continue;
            }

            key_type = key_type->GetUnaliased();
            value_type = value_type->GetUnaliased();

            if (m_key_type == BuiltinTypes::UNDEFINED) {
                m_key_type = key_type;
            } else {
                m_key_type = SymbolType::TypePromotion(m_key_type, key_type);
            }

            if (m_value_type == BuiltinTypes::UNDEFINED) {
                m_value_type = value_type;
            } else if (!m_value_type->TypeEqual(*value_type)) {
                m_value_type = SymbolType::TypePromotion(m_value_type, value_type);
            }

            m_replaced_keys.PushBack(CloneAstNode(key_value_pair.first));
            m_replaced_values.PushBack(CloneAstNode(key_value_pair.second));
        }
    } else {
        m_key_type = BuiltinTypes::ANY;
        m_value_type = BuiltinTypes::ANY;
    }

    // if either key or value type is undefined, set it to `Any`

    if (m_key_type == BuiltinTypes::UNDEFINED || m_value_type == BuiltinTypes::UNDEFINED) {
        // add error
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_determine_implicit_type,
            m_location
        ));
    }

    for (SizeType i = 0; i < m_replaced_keys.Size(); ++i) {
        auto &key = m_replaced_keys[i];
        AssertThrow(key != nullptr);

        auto &value = m_replaced_values[i];
        AssertThrow(value != nullptr);

        auto &replaced_key = m_replaced_keys[i];
        AssertThrow(replaced_key != nullptr);

        auto &replaced_value = m_replaced_values[i];
        AssertThrow(replaced_value != nullptr);

        if (SymbolTypePtr_t key_type = key->GetExprType()) {
            if (!key_type->TypeEqual(*m_key_type)) {
                // Add cast
                replaced_key.Reset(new AstAsExpression(
                    replaced_key,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_key_type,
                            key->GetLocation()
                        )),
                        key->GetLocation()
                    )),
                    key->GetLocation()
                ));
            }
        }

        if (SymbolTypePtr_t value_type = value->GetExprType()) {
            if (!value_type->TypeEqual(*m_value_type)) {
                // Add cast
                replaced_value.Reset(new AstAsExpression(
                    replaced_value,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_value_type,
                            value->GetLocation()
                        )),
                        value->GetLocation()
                    )),
                    value->GetLocation()
                ));
            }
        }
    }

    // @TODO: Cache generic instance types
    m_map_type_expr.Reset(new AstPrototypeSpecification(
        RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            RC<AstVariable>(new AstVariable(
                "Map",
                m_location
            )),
            {
                RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_key_type,
                        m_location
                    )),
                    false,
                    false,
                    false,
                    false,
                    "K",
                    m_location
                )),

                RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_value_type,
                        m_location
                    )),
                    false,
                    false,
                    false,
                    false,
                    "V",
                    m_location
                ))
            },
            m_location
        )),
        m_location
    ));

    m_map_type_expr->Visit(visitor, mod);

    SymbolTypePtr_t map_type = m_map_type_expr->GetHeldType();
    
    if (map_type == nullptr) {
        // add error

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location
        ));

        return;
    }

    map_type = map_type->GetUnaliased();

    m_expr_type = map_type;

    Array<RC<AstExpression>> key_value_array_expressions;
    key_value_array_expressions.Reserve(m_replaced_keys.Size());

    for (SizeType i = 0; i < m_replaced_keys.Size(); ++i) {
        auto &key = m_replaced_keys[i];
        AssertThrow(key != nullptr);

        auto &value = m_replaced_values[i];
        AssertThrow(value != nullptr);

        Array<RC<AstExpression>> key_value_pair;
        key_value_pair.Reserve(2);

        key_value_pair.PushBack(key);
        key_value_pair.PushBack(value);

        key_value_array_expressions.PushBack(RC<AstArrayExpression>(new AstArrayExpression(
            key_value_pair,
            m_location
        )));
    }

    m_array_expr.Reset(new AstArrayExpression(
        key_value_array_expressions,
        m_location
    ));
    
    m_array_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstHashMap::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_map_type_expr != nullptr);
    chunk->Append(m_map_type_expr->Build(visitor, mod));
    
    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // keep type obj in memory so we can do Map<K, V>.from(...), so push it to the stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_push));
    }

    int class_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    AssertThrow(m_array_expr != nullptr);
    chunk->Append(m_array_expr->Build(visitor, mod));

    const UInt8 array_reg = rp;

    // move array to stack
    { 
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_push));
    }
    
    int array_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // increment stack size
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    { // load class from stack back into register
        auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
        instr_load_offset->GetBuilder()
            .Load(rp)
            .Local()
            .ByOffset(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - class_stack_location);

        chunk->Append(std::move(instr_load_offset));
    }

    { // load member `from` from array type expr
        constexpr UInt32 from_hash = hash_fnv_1("from");

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, from_hash));
    }

    // Here map class and array should be the 2 items on the stack
    // so we call `from`, and the class will be the first arg, and the array will be the second arg

    { // call the `from` method
        chunk->Append(Compiler::BuildCall(
            visitor,
            mod,
            nullptr, // no target -- handled above
            UInt8(2) // self, array
        ));
    }

    // decrement stack size for array type expr
    chunk->Append(BytecodeUtil::Make<PopLocal>(2));

    // pop array and type from stack
    for (UInt i = 0; i < 2; i++) {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

void AstHashMap::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_map_type_expr != nullptr) {
        m_map_type_expr->Optimize(visitor, mod);
    }

    if (m_array_expr != nullptr) {
        m_array_expr->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstHashMap::Clone() const
{
    return CloneImpl();
}

Tribool AstHashMap::IsTrue() const
{
    return Tribool::True();
}

bool AstHashMap::MayHaveSideEffects() const
{
    // return true because we have calls to __map_new and __map_set
    return true;
}

SymbolTypePtr_t AstHashMap::GetExprType() const
{
    if (m_expr_type == nullptr) {
        return BuiltinTypes::UNDEFINED;
    }

    return m_expr_type;
}

} // namespace hyperion::compiler
