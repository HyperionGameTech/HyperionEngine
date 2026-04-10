#include "AxPaths.hpp"

#include <Core/FxFilesystemIO.hpp>

// TODO: Find asset path based on current dir
const char* FxAssetPath(AxPathQuery query)
{
    switch (query) {
    case AxPathQuery::eShaders:
        return FX_BASE_DIR "/Shaders/";
    case AxPathQuery::eDataPacks:
        return FX_BASE_DIR "/build/";
    }
}
