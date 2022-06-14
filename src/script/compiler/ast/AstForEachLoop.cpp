#include <script/compiler/ast/AstForEachLoop.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

namespace hyperion::compiler {

AstForEachLoop::AstForEachLoop(const std::vector<std::shared_ptr<AstParameter>> &params,
    const std::shared_ptr<AstExpression> &iteree,
    const std::shared_ptr<AstBlock> &block,
    const SourceLocation &location)
    : AstStatement(location),
      m_params(params),
      m_iteree(iteree),
      m_block(block)
{
}

void AstForEachLoop::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_iteree != nullptr);

    std::vector<std::shared_ptr<AstArgument>> action_args = {
        std::shared_ptr<AstArgument>(new AstArgument(
            m_iteree,
            false,
            false,
            "",
            m_iteree->GetLocation()
        )),
        std::shared_ptr<AstArgument>(new AstArgument(
            std::shared_ptr<AstFunctionExpression>(new AstFunctionExpression(
                m_params,
                nullptr,
                m_block,
                false,
                false,
                false,
                m_location
            )),
            false,
            false,
            "",
            m_location
        )),
    };

    m_expr = visitor->GetCompilationUnit()->GetAstNodeBuilder()
        .Module("events")
        .Function("call_action")
        .Call(action_args);

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstForEachLoop::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstForEachLoop::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstForEachLoop::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
