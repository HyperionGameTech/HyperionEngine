#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

namespace Hyperion {

AstTrue::AstTrue(const SourceLocation& location)
    : AstConstant(ConstantValue(true, CBS_8), location)
{
}

UniquePtr<Buildable> AstTrue::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

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

const SymbolType* AstTrue::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

} // namespace Hyperion
