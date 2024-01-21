#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

namespace hyperion::compiler {

AstTrue::AstTrue(const SourceLocation &location)
    : AstConstant(location)
{
}

std::unique_ptr<Buildable> AstTrue::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    const UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, true);
}

RC<AstStatement> AstTrue::Clone() const
{
    return CloneImpl();
}

Tribool AstTrue::IsTrue() const
{
    return Tribool::True();
}

bool AstTrue::IsNumber() const
{
    return false;
}

hyperion::Int32 AstTrue::IntValue() const
{
    return 1;
}

hyperion::Float32 AstTrue::FloatValue() const
{
    return 1.0f;
}

SymbolTypePtr_t AstTrue::GetExprType() const
{
    return BuiltinTypes::BOOLEAN;
}

RC<AstConstant> AstTrue::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_logical_and:
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
            if (dynamic_cast<const AstTrue*>(right) != nullptr) {
                return RC<AstTrue>(new AstTrue(m_location));
            }

            return RC<AstFalse>(new AstFalse(m_location));

        case OP_logical_not:
            return RC<AstFalse>(new AstFalse(m_location));

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
