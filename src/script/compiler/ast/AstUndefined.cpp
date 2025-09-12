#include <script/compiler/ast/AstUndefined.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion {

AstUndefined::AstUndefined(const SourceLocation& location)
    : AstConstant(ConstantValue(INVALID_CONSTANT_NUMBER), location)
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

SymbolTypeRef AstUndefined::GetExprType() const
{
    return BuiltinTypes::s_errorType;
}

} // namespace hyperion
