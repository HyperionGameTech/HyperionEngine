#include <script/compiler/ast/AstFunctionDefinition.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstFunctionDefinition::AstFunctionDefinition(
    const String& name,
    const RC<AstFunctionExpression>& expr,
    const SourceLocation& location)
    : AstDeclaration(name, location),
      m_expr(expr)
{
}

void AstFunctionDefinition::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    AstDeclaration::Visit(visitor, mod);

    if (m_identifier != nullptr)
    {
        // functions are implicitly const
        m_identifier->GetFlags() |= FLAG_CONST;
        m_identifier->SetSymbolType(m_expr->GetExprType());
        m_identifier->SetCurrentValue(m_expr);
    }
}

UniquePtr<Buildable> AstFunctionDefinition::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Function definition: " + ToString()));

    if (!Config::cullUnusedObjects || m_identifier->GetUseCount() > 0)
    {
        // get current stack size
        int stackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // set identifier stack location
        m_identifier->SetStackLocation(stackLocation);

        chunk->Append(BytecodeUtil::Make<Comment>("Function " + m_name + " will be stored at stack location " + String::ToString(stackLocation)));

        // increment stack size before we build the expression
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

        // build function expression
        chunk->Append(m_expr->Build(visitor, mod));

        // get active register
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // store on stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    return chunk;
}

void AstFunctionDefinition::Optimize(AstVisitor* visitor, Module* mod)
{
    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstFunctionDefinition::Clone() const
{
    return CloneImpl();
}

String AstFunctionDefinition::ToString() const
{
    return "function " + m_name + (m_expr ? m_expr->ToString() : "(<params>) { ... }");
}

} // namespace hyperion::compiler
