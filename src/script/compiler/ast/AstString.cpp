#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

namespace hyperion::compiler {

AstString::AstString(const std::string &value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value),
      m_static_id(0)
{
}

std::unique_ptr<Buildable> AstString::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_string = BytecodeUtil::Make<BuildableString>();
    instr_string->reg = rp;
    instr_string->value = m_value;

    return instr_string;
}

RC<AstStatement> AstString::Clone() const
{
    return CloneImpl();
}

Tribool AstString::IsTrue() const
{
    // strings evaluate to true
    return Tribool::True();
}

bool AstString::IsNumber() const
{
    return false;
}

hyperion::Int32 AstString::IntValue() const
{
    // not valid
    return 0;
}

hyperion::Float32 AstString::FloatValue() const
{
    // not valid
    return 0.0f;
}

SymbolTypePtr_t AstString::GetExprType() const
{
    return BuiltinTypes::STRING;
}

RC<AstConstant> AstString::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_logical_and:
            // literal strings evaluate to true.
            switch (right->IsTrue()) {
                case TRI_TRUE:
                    return RC<AstTrue>(new AstTrue(m_location));
                case TRI_FALSE:
                    return RC<AstFalse>(new AstFalse(m_location));
                case TRI_INDETERMINATE:
                    return nullptr;
            }

        case OP_logical_or:
            return RC<AstTrue>(new AstTrue(m_location));

        case OP_equals:
            if (const AstString *right_string = dynamic_cast<const AstString*>(right)) {
                if (m_value == right_string->GetValue()) {
                    return RC<AstTrue>(new AstTrue(m_location));
                } else {
                    return RC<AstFalse>(new AstFalse(m_location));
                }
            }

            return nullptr;

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
