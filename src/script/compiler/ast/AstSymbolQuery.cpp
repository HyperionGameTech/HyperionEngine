#include <script/compiler/ast/AstSymbolQuery.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Script.hpp>
#include <script/SourceFile.hpp>

#include <system/Debug.hpp>

#include <asset/ByteReader.hpp>
#include <core/lib/UniquePtr.hpp>

#include "AstFalse.hpp"
#include "AstTrue.hpp"

namespace hyperion::compiler {

using v2::Script;

AstSymbolQuery::AstSymbolQuery(
    const String &command_name,
    const RC<AstExpression> &expr,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_command_name(command_name),
    m_expr(expr)
{
}

void AstSymbolQuery::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_symbol_type = BuiltinTypes::UNDEFINED;

    if (m_command_name == "inspect_type") {
        SymbolTypePtr_t expr_type = m_expr->GetExprType();
        AssertThrow(expr_type != nullptr);

        m_result_value = RC<AstString>::Construct(expr_type->ToString(), m_location);
    } else if (m_command_name == "log") {
        const auto *value_of = m_expr->GetDeepValueOf();

        if (value_of) {
            const auto *string_value = dynamic_cast<const AstString *>(value_of);

            if (string_value) {
                DebugLog(LogType::Info, "$meta::log(): %s\n", string_value->GetValue().Data());
            } else {
                DebugLog(LogType::Warn, "$meta::log(): Not a constant string\n");
            }
        } else {
            DebugLog(LogType::Warn, "$meta::log(): No value found for expression\n");
        }
    } else if (m_command_name == "fields") {
        SymbolTypePtr_t expr_type = m_expr->GetExprType();
        AssertThrow(expr_type != nullptr);

        Array<RC<AstExpression>> field_names;

        for (const auto &member : expr_type->GetMembers()) {
            field_names.PushBack(RC<AstString>::Construct(std::get<0>(member), m_location));
        }

        m_result_value = RC<AstArrayExpression>(new AstArrayExpression(
            field_names,
            m_location
        ));

        m_result_value->Visit(visitor, mod);
    } else if (m_command_name == "compiles") {
        const auto *value_of = m_expr->GetDeepValueOf();
        const AstString *string_value = nullptr;

        if (value_of) {
            string_value = dynamic_cast<const AstString *>(value_of);
        }

        if (!string_value) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                m_location
            ));

            return;
        }

        String value = string_value->GetValue();

        ByteBuffer byte_buffer;
        byte_buffer.SetData(value.Size(), value.Data());

        SourceFile source_file(value, value.Size());
        source_file.ReadIntoBuffer(byte_buffer);

        UniquePtr<Script> script(new Script(source_file));

        if (script->Compile()) {
            script->Bake();

            m_result_value = RC<AstTrue>(new AstTrue(m_location));
        } else {
            m_result_value = RC<AstFalse>(new AstFalse(m_location));
        }
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_invalid_symbol_query,
            m_location,
            m_command_name
        ));
    }
}

std::unique_ptr<Buildable> AstSymbolQuery::Build(AstVisitor *visitor, Module *mod)
{
    if (m_result_value != nullptr) {
        return m_result_value->Build(visitor, mod);
    }

    return nullptr;
}

void AstSymbolQuery::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstSymbolQuery::Clone() const
{
    return CloneImpl();
}

Tribool AstSymbolQuery::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstSymbolQuery::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstSymbolQuery::GetExprType() const
{
    if (m_result_value != nullptr) {
        return m_result_value->GetExprType();
    }

    return BuiltinTypes::UNDEFINED;
}

const AstExpression *AstSymbolQuery::GetValueOf() const
{
    if (m_result_value != nullptr) {
        return m_result_value.Get();
    }

    return this;
}

} // namespace hyperion::compiler
