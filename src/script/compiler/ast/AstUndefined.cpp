#include <script/compiler/ast/AstUndefined.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion {

AstUndefined::AstUndefined(const SourceLocation& location)
    : AstConstant(location)
{
}

UniquePtr<Buildable> AstUndefined::Build(AstVisitor* visitor, Module* mod)
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

hyperion::int32 AstUndefined::IntValue() const
{
    return 0;
}

float AstUndefined::FloatValue() const
{
    return 0.0f;
}

SymbolTypeRef AstUndefined::GetExprType() const
{
    return BuiltinTypes::UNDEFINED;
}

RC<AstConstant> AstUndefined::HandleOperator(Operators opType, const AstConstant* right) const
{
    return nullptr;
}

} // namespace hyperion
