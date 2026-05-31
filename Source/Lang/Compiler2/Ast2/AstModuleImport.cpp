#include <Lang/compiler/ast/AstModuleImport.hpp>
#include <Lang/SourceFile.hpp>
#include <Lang/compiler/Lexer.hpp>
#include <Lang/compiler/Parser.hpp>
#include <Lang/compiler/SemanticAnalyzer.hpp>
#include <Lang/compiler/Optimizer.hpp>

#include <Core/filesystem/FsUtil.hpp>
#include <Core/filesystem/FilePath.hpp>

#include <Core/io/BufferedByteReader.hpp>

namespace Hyperion {

static constexpr const char* WildcardImport = "*";

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

    if (m_left == WildcardImport)
    {
        // Import all nested modules
        for (Module* nested : mod->CollectNestedModules())
        {
            AstImport::CopyModules(
                visitor,
                nested,
                false);
        }

        // Import all identifiers and types
        Scope& scope = mod->scopeTree.Top();

        for (const RC<Identifier>& ident : scope.identifierTable.identifiers)
        {
            m_foundSymbols.PushBack(Symbol(ident));
        }

        for (const SymbolType* type : scope.identifierTable.symbolTypes)
        {
            m_foundSymbols.PushBack(Symbol(type));
        }

        return;
    }

    Symbol foundSymbol;

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

                if (part->GetFoundSymbols().Any())
                {
                    m_foundSymbols.Concat(part->GetFoundSymbols());
                }
            }
        }
    }
    else if (m_rightParts.Empty() && (foundSymbol = mod->LookUpIdentifierOrSymbolType(m_left,
                                          /* includePlaceholderTypes */ false,
                                          /* thisScopeOnly */ true,
                                          /* outsideModules */ false)))
    {
        // pull the identifier into local scope
        m_foundSymbols.PushBack(std::move(foundSymbol));
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
        size_t slashIndex = m_location.GetFileName().FindLastIndex('/');
        slashIndex = (slashIndex == String::NotFound)
            ? m_location.GetFileName().FindLastIndex('\\')
            : MathUtil::Max(slashIndex, m_location.GetFileName().FindLastIndex('\\'));

        String currentDir;
        if (slashIndex != String::NotFound)
        {
            currentDir = m_location.GetFileName().Substr(0, slashIndex);
        }

        FlatSet<String> scanPaths;
        String foundPath;

        if (currentDir.Any())
        {
            // add current directory as first.
            scanPaths.Insert(currentDir);
        }

        // add this module's scan paths.
        for (const String& scanPath : mod->GetScanPaths())
        {
            scanPaths.Insert(scanPath);
        }

        // add global module's scan paths
        const FlatSet<String>& globalScanPaths =
            visitor->GetCompilationUnit()->GetGlobalModule()->GetScanPaths();

        for (const String& scanPath : globalScanPaths)
        {
            scanPaths.Insert(scanPath);
        }

        ByteReader* stream = nullptr;

        // iterate through library paths to try and find a file
        for (const String& scanPath : scanPaths)
        {
            const String& filename = first->GetLeft();
            const String ext = ".hyp";

            foundPath = FilePath::Join(scanPath, filename + ext);
            triedPaths.PushBack(foundPath);

            if (AstImport::TryOpenFile(foundPath, stream))
            {
                opened = true;
                break;
            }

            // try it without extension
            foundPath = FilePath::Join(scanPath, filename);
            triedPaths.PushBack(foundPath);

            if (AstImport::TryOpenFile(foundPath, stream))
            {
                opened = true;
                break;
            }
        }

        if (opened)
        {
            delete stream;
            stream = nullptr;

            AstImport::PerformImport(
                visitor,
                mod,
                foundPath);
        }
    }

    if (opened)
    {
        Array<Symbol> pulledInSymbols;

        for (const RC<AstModuleImportPart>& part : m_parts)
        {
            Assert(part != nullptr);
            part->Visit(visitor, mod);

            pulledInSymbols.Concat(part->GetFoundSymbols());
        }

        for (const Symbol& symbol : pulledInSymbols)
        {
            if (symbol.Is<RC<Identifier>>())
            {
                const RC<Identifier>& identifier = symbol.Get<RC<Identifier>>();

                if (!mod->scopeTree.Top().identifierTable.AddIdentifier(identifier))
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_redeclared_identifier,
                        m_location,
                        identifier->GetName()));
                }
            }
            else if (symbol.Is<const SymbolType*>())
            {
                const SymbolType* symbolType = symbol.Get<const SymbolType*>();

                if (mod->scopeTree.Top().identifierTable.LookupSymbolType(symbolType->GetName()) != nullptr)
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_redeclared_identifier_type,
                        m_location,
                        symbolType->GetName()));

                    continue;
                }

                mod->scopeTree.Top().identifierTable.AddSymbolType(const_cast<SymbolType*>(symbolType));
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
    }
    else
    {
        String triedPathsString;
        triedPathsString += "[";

        for (size_t i = 0; i < triedPaths.Size(); i++)
        {
            triedPathsString += "\"" + triedPaths[i] + "\"";

            if (i != triedPaths.Size() - 1)
            {
                triedPathsString += ", ";
            }
        }

        triedPathsString += "]";

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_could_not_find_module,
            m_location,
            first->GetLeft(),
            triedPathsString));
    }
}

RC<AstStatement> AstModuleImport::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
