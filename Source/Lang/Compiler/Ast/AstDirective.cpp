#include <Lang/Compiler/Ast/AstDirective.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/CompilerError.hpp>

#include <Core/Debug/Debug.hpp>

#include <AstDirective.generated.inl>

namespace Hyperion {

AstDirective::AstDirective(
    const String& key,
    const Array<String>& args,
    const SourceLocation& location)
    : AstStatement(location),
      m_key(key),
      m_args(args)
{
}

void AstDirective::Visit(AstVisitor* visitor, Module* mod)
{
    if (m_key == "importpath")
    {
        // library names should be supplied in arguments as all strings
        if (m_args.Empty())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_custom_error,
                m_location,
                String("'importpath' directive requires path names to be provided (e.g '#importpath \"../path\" \"../other/path\"')")));
        }
        else
        {
            // split current file path and remove file name
            const String currentDir = String::Join(m_location.GetFileName().Split('/', '\\').Slice(0, -2), '/') + "/";

            for (const String& pathArg : m_args)
            {
                const String scanPath = currentDir + pathArg;

                mod->AddScanPath(scanPath);
            }
        }
    }
    else
    {
        CompilerError error(
            LEVEL_ERROR,
            Msg_unknown_directive,
            m_location,
            m_key);

        visitor->GetCompilationUnit()->GetErrorList().AddError(error);
    }
}

UniquePtr<Buildable> AstDirective::Build(AstVisitor* visitor, Module* mod)
{
    return nullptr;
}

void AstDirective::Optimize(AstVisitor* visitor, Module* mod)
{
}

RC<AstStatement> AstDirective::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
