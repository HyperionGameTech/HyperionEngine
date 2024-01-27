#include <script/compiler/ast/AstHashMap.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
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
                "map",
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

    { // create a block with a call to __map_new and member assignments
        m_block.Reset(new AstBlock(m_location));

        m_block->AddChild(RC<AstVariableDeclaration>(new AstVariableDeclaration(
            "__map",
            nullptr,
            visitor->GetCompilationUnit()->GetAstNodeBuilder()
                .Module(Config::global_module_name)
                .Function("__map_new")
                .Call({ }),
            IdentifierFlags::FLAG_NONE,
            m_location
        )));

        for (SizeType i = 0; i < m_replaced_keys.Size(); ++i) {
            auto &key = m_replaced_keys[i];
            AssertThrow(key != nullptr);

            auto &value = m_replaced_values[i];
            AssertThrow(value != nullptr);

            m_block->AddChild(visitor->GetCompilationUnit()->GetAstNodeBuilder()
                .Module(Config::global_module_name)
                .Function("__map_set")
                .Call({
                    RC<AstArgument>(new AstArgument(
                        RC<AstVariable>(new AstVariable(
                            "__map",
                            m_location
                        )),
                        false,
                        false,
                        false,
                        false,
                        "map",
                        m_location
                    )),
                    RC<AstArgument>(new AstArgument(
                        key,
                        false,
                        false,
                        false,
                        false,
                        "key",
                        m_location
                    )),
                    RC<AstArgument>(new AstArgument(
                        value,
                        false,
                        false,
                        false,
                        false,
                        "value",
                        m_location
                    ))
                }));
        }

        // return __map
        m_block->AddChild(RC<AstVariable>(new AstVariable(
            "__map",
            m_location
        )));

        m_block->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstHashMap::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_map_type_expr != nullptr) {
        chunk->Append(m_map_type_expr->Build(visitor, mod));
    }

    AssertThrow(m_block != nullptr);
    chunk->Append(m_block->Build(visitor, mod));

    return chunk;
}

void AstHashMap::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_map_type_expr != nullptr) {
        m_map_type_expr->Optimize(visitor, mod);
    }

    if (m_block != nullptr) {
        m_block->Optimize(visitor, mod);
    }

    for (auto &key : m_replaced_keys) {
        AssertThrow(key != nullptr);
        key->Optimize(visitor, mod);
    }

    for (auto &value : m_replaced_values) {
        AssertThrow(value != nullptr);
        value->Optimize(visitor, mod);
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
