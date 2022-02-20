#ifndef WAV_LOADER_H
#define WAV_LOADER_H

#include "../asset/asset_loader.h"

namespace hyperion {
class WavLoader : public AssetLoader {
public:
    virtual Result LoadFromFile(const std::string &) override;
};
} // namespace hyperion

#endif
