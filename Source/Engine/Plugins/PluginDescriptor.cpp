/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginDescriptor.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>

#include <Core/IO/ByteReader.hpp>

#include <PluginDescriptor.generated.inl>

namespace Hyperion {

TResult<PluginDescriptor> PluginDescriptor::Read(const FilePath& descriptorPath)
{
    if (!descriptorPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Plugin descriptor does not exist at {}", descriptorPath);
    }

    FileByteReader stream { descriptorPath };

    HMF::ErrorList errorList;
    HMF::ParseResult parseResult = HMF::Parse(descriptorPath, stream, &errorList);

    if (parseResult.HasError() || !parseResult.GetValue().Is<PluginDescriptor>())
    {
        String parseMessages;
        errorList.WriteAllMessages(parseMessages);

        return HYP_MAKE_ERROR(Error, "Failed to read plugin descriptor {}: {}", descriptorPath, parseMessages);
    }

    PluginDescriptor descriptor = parseResult.GetValue().Get<PluginDescriptor>();
    descriptor.directory = descriptorPath.BasePath();

    return descriptor;
}

} // namespace Hyperion
