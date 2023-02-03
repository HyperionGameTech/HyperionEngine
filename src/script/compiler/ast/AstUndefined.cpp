#include <script/compiler/ast/AstUndefined.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion::compiler {

AstUndefined::AstUndefined(const SourceLocation &location)
    : AstConstant(location)
{
}

std::unique_ptr<Buildable> AstUndefined::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

RC<AstStatement> AstUndefined::Clone() const
{
    return CloneImpl();
}

Tribool AstUndefined::IsTrue() const
{
    return Tribool::False();
}

bool AstUndefined::IsNumber() const
{
    return false;
}

hyperion::Int32 AstUndefined::IntValue() const
{
    return 0;
}

hyperion::Float32 AstUndefined::FloatValue() const
{
    return 0.0f;
}

SymbolTypePtr_t AstUndefined::GetExprType() const
{
    return BuiltinTypes::UNDEFINED;
}

RC<AstConstant> AstUndefined::HandleOperator(Operators op_type, const AstConstant *right) const
{
    return nullptr;
}

} // namespace hyperion::compiler
