#include <Lang/Compiler/Ast/AstUndefined.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <AstUndefined.generated.inl>

namespace Hyperion {

AstUndefined::AstUndefined(const SourceLocation& location)
    : AstConstant(ConstantValue(INVALID_CONSTANT_NUMBER), location)
{
}

UniquePtr<Buildable> AstUndefined::Build(AstVisitor* visitor, Module* mod)
{
    return nullptr;
}

SharedPtr<AstStatement> AstUndefined::Clone() const
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

const SymbolType* AstUndefined::GetExprType() const
{
    return BuiltinTypes::s_errorType;
}

} // namespace Hyperion
