#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/Types.hpp>

#include <fstream>
#include <iostream>
#include <functional>

namespace Hyperion {

AstImport::AstImport(const SourceLocation& location)
    : AstStatement(location)
{
}

void AstImport::CopyModules(
    AstVisitor* visitor,
    Module* modToCopy,
    bool updateTreeLink)
{
    Assert(visitor != nullptr);
    Assert(modToCopy != nullptr);
    Assert(visitor->GetCompilationUnit()->GetCurrentModule() != nullptr);

    if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(modToCopy->GetName()) != nullptr)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_module_already_defined,
            modToCopy->GetLocation(),
            modToCopy->GetName()));

        return;
    }

    // add this module to the compilation unit
    visitor->GetCompilationUnit()->moduleTree.Open(modToCopy);
    // open scope for module
    // mod->scopeTree.Open(Scope());

    if (updateTreeLink)
    {
        modToCopy->SetImportTreeLink(visitor->GetCompilationUnit()->moduleTree.TopNode());
    }

    // function to copy nested modules
    std::function<void(TreeNode<Module*>*)> copyNodes = [visitor, &copyNodes, &updateTreeLink](TreeNode<Module*>* link)
    {
        Assert(link != nullptr);
        Assert(link->Get() != nullptr);

        for (auto* sibling : link->m_siblings)
        {
            Assert(sibling != nullptr);

            if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(sibling->Get()->GetName()) != nullptr)
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_module_already_defined,
                    sibling->Get()->GetLocation(),
                    sibling->Get()->GetName()));
            }
            else
            {
                visitor->GetCompilationUnit()->moduleTree.Open(sibling->Get());

                if (updateTreeLink)
                {
                    sibling->Get()->SetImportTreeLink(sibling);
                }

                copyNodes(sibling);

                visitor->GetCompilationUnit()->moduleTree.Close();
            }
        }
    };

    // copy all nested modules
    copyNodes(modToCopy->GetImportTreeLink());

    // close scope for module
    // mod->scopeTree.Close();

    // close module
    visitor->GetCompilationUnit()->moduleTree.Close();
}

bool AstImport::TryOpenFile(const String& path, BufferedReader& outReader)
{
    FileBufferedReaderSource* source = new FileBufferedReaderSource { FilePath(path) };
    outReader = BufferedReader(source);

    if (!outReader.IsOpen())
    {
        delete source;

        return false;
    }

    return true;
}

void AstImport::PerformImport(
    AstVisitor* visitor,
    Module* mod,
    const String& filepath)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    if (!mod->IsInGlobalScope())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_import_outside_global,
            m_location));

        return;
    }

    // parse path into vector
    Array<String> pathParts = filepath.Split('\\', '/');
    // canonicalize the vector
    pathParts = StringUtil::CanonicalizePath(pathParts);
    // put it back into a string
    const String canonPath = String::Join(pathParts, '/');

    // first, check if the file has already been imported somewhere in this compilation unit
    const auto it = visitor->GetCompilationUnit()->importedModules.Find(canonPath);
    if (it != visitor->GetCompilationUnit()->importedModules.End())
    {
        // imported file found, so just re-open all
        // modules that belong to the file into this scope

        // TODO: Fix issues with duplicated symbols...
        for (Module* mod : it->second)
        {
            AstImport::CopyModules(visitor, mod, false);
        }
    }
    else
    {
        // file hasn't been imported, so open it
        BufferedReader reader;

        if (!TryOpenFile(filepath, reader))
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_could_not_open_file,
                m_location,
                filepath));

            return;
        }

        SizeType max = reader.Max();
        reader.Seek(0);

        SourceFile sourceFile(filepath, max);

        ByteBuffer temp = reader.ReadBytes(max);

        sourceFile.ReadIntoBuffer(temp);

        delete reader.GetSource();

        // use the lexer and parser on this file buffer
        TokenStream tokenStream(TokenStreamInfo {
            filepath });

        Lexer lexer(SourceStream(&sourceFile), &tokenStream, visitor->GetCompilationUnit());
        lexer.Analyze();

        Parser parser(&m_astIterator, &tokenStream, visitor->GetCompilationUnit());
        parser.Parse();

        SemanticAnalyzer semanticAnalyzer(&m_astIterator, visitor->GetCompilationUnit());
        semanticAnalyzer.Analyze();

        /*if (makeParentModule) {
            visitor->GetCompilationUnit()->moduleTree.Close();
        }*/
    }
}

UniquePtr<Buildable> AstImport::Build(AstVisitor* visitor, Module* mod)
{
    m_astIterator.ResetPosition();

    // compile the imported module
    Compiler compiler(&m_astIterator, visitor->GetCompilationUnit());
    return compiler.Compile();
}

void AstImport::Optimize(AstVisitor* visitor, Module* mod)
{
    m_astIterator.ResetPosition();

    // optimize the imported module
    Optimizer optimizer(&m_astIterator, visitor->GetCompilationUnit());
    optimizer.Optimize();
}

} // namespace Hyperion
