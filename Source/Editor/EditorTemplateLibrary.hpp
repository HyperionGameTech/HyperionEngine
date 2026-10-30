/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class Node;
class AssetObject;

class EDITOR_API EditorTemplateLibrary final
{
public:
    // where SaveTemplate writes the user's templates
    static const FilePath& GetDirectory();

    // GetDirectory, then the Templates folder of each enabled plugin; the first with a template of a name provides it
    static Array<FilePath> GetSearchDirectories();

    // empty when no search directory has the template
    static FilePath FindTemplateDirectory(Name templateName);

    static Array<Name> GetTemplateNames();

    static bool HasTemplate(Name templateName);

    static bool IsValidTemplateName(const String& templateName);

    static Result SaveTemplate(Name templateName, const Handle<Node>& root);

    static TResult<Handle<Node>> InstantiateTemplate(Name templateName, Array<Handle<AssetObject>>* outImportedAssets = nullptr);
};

} // namespace Hyperion
