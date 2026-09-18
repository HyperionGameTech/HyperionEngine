/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/Preview/MaterialPreviewRenderer.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT uint64 EditorSubsystem_CopyMaterialPreviewFrame(
        EditorSubsystem* pSubsystem,
        void* pDest,
        uint64 destSize,
        uint32* pOutWidth,
        uint32* pOutHeight)
    {
        uint32 width = 0;
        uint32 height = 0;
        uint64 bytesWritten = 0;

        if (pSubsystem != nullptr)
        {
            if (MaterialPreviewRenderer* renderer = pSubsystem->GetMaterialPreviewRenderer())
            {
                bytesWritten = uint64(renderer->CopyLatestFrame(pDest, size_t(destSize), width, height));
            }
        }

        if (pOutWidth != nullptr)
        {
            *pOutWidth = width;
        }

        if (pOutHeight != nullptr)
        {
            *pOutHeight = height;
        }

        return bytesWritten;
    }
} // extern "C"
