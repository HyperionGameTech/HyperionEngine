#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

AstNil::AstNil(const SourceLocation& location)
    : AstConstant(ConstantValue(int64(0), CBS_32), location)
{
}

UniquePtr<Buildable> AstNil::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstNull>(rp);
}

RC<AstStatement> AstNil::Clone() const
{
    return CloneImpl();
}

Tribool AstNil::IsTrue() const
{
    return Tribool::False();
}

bool AstNil::IsNumber() const
{
    return false;
}

const SymbolType* AstNil::GetExprType() const
{
    return BuiltinTypes::s_nullType;
}

} // namespace Hyperion
