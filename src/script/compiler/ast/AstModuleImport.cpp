#include <script/compiler/ast/AstModuleImport.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/io/BufferedByteReader.hpp>

namespace hyperion {

AstModuleImportPart::AstModuleImportPart(
    const String& left,
    const Array<RC<AstModuleImportPart>>& rightParts,
    const SourceLocation& location)
    : AstStatement(location),
      m_left(left),
      m_rightParts(rightParts),
      m_pullInModules(true)
{
}

void AstModuleImportPart::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    RC<Identifier> leftIdentifier;

    if (Module* thisModule = mod->LookupNestedModule(m_left))
    {
        if (m_pullInModules && m_rightParts.Empty())
        {
            // pull this module into scope
            AstImport::CopyModules(
                visitor,
                thisModule,
                false);
        }
        else
        {
            // get nested items
            for (const RC<AstModuleImportPart>& part : m_rightParts)
            {
                Assert(part != nullptr);
                part->Visit(visitor, thisModule);

                if (part->GetIdentifiers().Any())
                {
                    for (const auto& identifier : part->GetIdentifiers())
                    {
                        m_identifiers.PushBack(identifier);
                    }
                }
            }
        }
    }
    else if (m_rightParts.Empty() && (leftIdentifier = mod->LookUpIdentifier(m_left, false)))
    {
        // pull the identifier into local scope
        m_identifiers.PushBack(std::move(leftIdentifier));
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_find_nested_module,
            m_location,
            m_left,
            mod->GetName()));
    }
}

UniquePtr<Buildable> AstModuleImportPart::Build(AstVisitor* visitor, Module* mod)
{
    return nullptr;
}

void AstModuleImportPart::Optimize(AstVisitor* visitor, Module* mod)
{
}

RC<AstStatement> AstModuleImportPart::Clone() const
{
    return CloneImpl();
}

AstModuleImport::AstModuleImport(
    const Array<RC<AstModuleImportPart>>& parts,
    const SourceLocation& location)
    : AstImport(location),
      m_parts(parts)
{
}

void AstModuleImport::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_parts.Empty());

    const RC<AstModuleImportPart>& first = m_parts[0];
    Assert(first != nullptr);

    bool opened = false;

    // already imported into this module, set opened to true
    if (mod->LookupNestedModule(first->GetLeft()) != nullptr)
    {
        opened = true;
    }

    // do not pull module into scope for single imports
    // i.e `import range` will just import the file
    if (first->GetParts().Empty())
    {
        first->SetPullInModules(false);
    }

    Array<String> triedPaths;

    // if this is not a direct import (i.e `import range`),
    // we will allow duplicates in imports like `import range::{_Detail_}`
    // and we won't import the 'range' module again
    if (first->GetParts().Empty() || !opened)
    {
        // find the folder which the current file is in
        SizeType slashIndex = m_location.GetFileName().FindLastIndex('/');
        slashIndex = (slashIndex == String::notFound)
            ? m_location.GetFileName().FindLastIndex('\\')
            : MathUtil::Max(slashIndex, m_location.GetFileName().FindLastIndex('\\'));

        String currentDir;
        if (slashIndex != String::notFound)
        {
            currentDir = m_location.GetFileName().Substr(0, slashIndex);
        }

        BufferedReader reader;

        FlatSet<String> scanPaths;
        String foundPath;

        // add current directory as first.
        scanPaths.Insert(currentDir);

        // add this module's scan paths.
        for (const auto& scanPath : mod->GetScanPaths())
        {
            scanPaths.Insert(scanPath);
        }

        // add global module's scan paths
        const FlatSet<String>& globalScanPaths =
            visitor->GetCompilationUnit()->GetGlobalModule()->GetScanPaths();

        for (const auto& scanPath : globalScanPaths)
        {
            scanPaths.Insert(scanPath);
        }

        // iterate through library paths to try and find a file
        for (const String& scanPath : scanPaths)
        {
            const String& filename = first->GetLeft();
            const String ext = ".hypscript";

            foundPath = FilePath::Join(scanPath, filename + ext);
            triedPaths.PushBack(foundPath);

            if (AstImport::TryOpenFile(foundPath, reader))
            {
                opened = true;
                break;
            }

            // try it without extension
            foundPath = FilePath::Join(scanPath, filename);
            triedPaths.PushBack(foundPath);

            if (AstImport::TryOpenFile(foundPath, reader))
            {
                opened = true;
                break;
            }
        }

        if (opened)
        {
            delete reader.GetSource();

            AstImport::PerformImport(
                visitor,
                mod,
                foundPath);
        }
    }

    if (opened)
    {
        Array<RC<Identifier>> pulledInIdentifiers;

        for (const RC<AstModuleImportPart>& part : m_parts)
        {
            Assert(part != nullptr);
            part->Visit(visitor, mod);

            for (const RC<Identifier>& identifier : part->GetIdentifiers())
            {
                pulledInIdentifiers.PushBack(identifier);
            }
        }

        for (auto& identifier : pulledInIdentifiers)
        {
            if (!mod->scopeTree.Top().identifierTable.AddIdentifier(identifier))
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_redeclared_identifier,
                    m_location,
                    identifier->GetName()));
            }
        }
    }
    else
    {
        std::stringstream triedPathsString;
        triedPathsString << "[";

        for (SizeType i = 0; i < triedPaths.Size(); i++)
        {
            triedPathsString << '"' << triedPaths[i] << '"';

            if (i != triedPaths.Size() - 1)
            {
                triedPathsString << ", ";
            }
        }

        triedPathsString << "]";

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_find_module,
            m_location,
            first->GetLeft(),
            triedPathsString.str()));
    }
}

RC<AstStatement> AstModuleImport::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion
