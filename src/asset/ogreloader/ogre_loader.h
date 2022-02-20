#ifndef OGRE_LOADER_H
#define OGRE_LOADER_H

#include "../asset_loader.h"
#include "../../util/xml/sax_parser.h"

namespace hyperion {
class OgreLoader : public AssetLoader {
public:
    virtual Result LoadFromFile(const std::string &) override;
};
} // namespace hyperion

#endif
