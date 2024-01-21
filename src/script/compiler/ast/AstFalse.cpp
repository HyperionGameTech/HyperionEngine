#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <Types.hpp>

namespace hyperion::compiler {

AstFalse::AstFalse(const SourceLocation &location)
    : AstConstant(location)
{
}

std::unique_ptr<Buildable> AstFalse::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    const UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, false);
}

RC<AstStatement> AstFalse::Clone() const
{
    return CloneImpl();
}

Tribool AstFalse::IsTrue() const
{
    return Tribool::False();
}

bool AstFalse::IsNumber() const
{
    return false;
}

hyperion::Int32 AstFalse::IntValue() const
{
    return 0;
}

hyperion::Float32 AstFalse::FloatValue() const
{
    return 0.0f;
}

SymbolTypePtr_t AstFalse::GetExprType() const
{
    return BuiltinTypes::BOOLEAN;
}

RC<AstConstant> AstFalse::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_logical_and:
            return RC<AstFalse>(new AstFalse(m_location));

        case OP_logical_or:
            switch (right->IsTrue()) {
                case TRI_TRUE:
                    return RC<AstTrue>(new AstTrue(m_location));
                case TRI_FALSE:
                    return RC<AstFalse>(new AstFalse(m_location));
                case TRI_INDETERMINATE:
                    return nullptr;
            }

        case OP_equals:
            if (dynamic_cast<const AstFalse*>(right) != nullptr) {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            return RC<AstFalse>(new AstFalse(m_location));

        case OP_logical_not:
            return RC<AstTrue>(new AstTrue(m_location));

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
