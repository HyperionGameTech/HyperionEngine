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
    static const FilePath& GetDirectory();

    static Array<Name> GetTemplateNames();

    static bool HasTemplate(Name templateName);

    static bool IsValidTemplateName(const String& templateName);

    static Result SaveTemplate(Name templateName, const Handle<Node>& root);

    static TResult<Handle<Node>> InstantiateTemplate(Name templateName, Array<Handle<AssetObject>>* outImportedAssets = nullptr);
};

} // namespace Hyperion
