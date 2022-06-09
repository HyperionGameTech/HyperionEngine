#include <script/compiler/ast/AstPrintStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion::compiler {

AstPrintStatement::AstPrintStatement(const std::shared_ptr<AstArgumentList> &arg_list,
        const SourceLocation &location)
    : AstStatement(location),
      m_arg_list(arg_list)
{
}

void AstPrintStatement::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_arg_list != nullptr);
    m_arg_list->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstPrintStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_arg_list != nullptr);

    // accept each argument
    for (auto &arg : m_arg_list->GetArguments()) {
        chunk->Append(arg->Build(visitor, mod));

        // get active register
        uint8_t rp = visitor->GetCompilationUnit()->
            GetInstructionStream().GetCurrentRegister();

        auto instr_echo = BytecodeUtil::Make<RawOperation<>>();
        instr_echo->opcode = ECHO;
        instr_echo->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_echo));
    }

    // print newline
    auto instr_echo_newline = BytecodeUtil::Make<RawOperation<>>();
    instr_echo_newline->opcode = ECHO_NEWLINE;
    chunk->Append(std::move(instr_echo_newline));

    return std::move(chunk);
}

void AstPrintStatement::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_arg_list != nullptr);
    m_arg_list->Optimize(visitor, mod);
}

Pointer<AstStatement> AstPrintStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
