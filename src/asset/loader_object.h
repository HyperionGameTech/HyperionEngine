#ifndef HYPERION_V2_LOADER_OBJECT_H
#define HYPERION_V2_LOADER_OBJECT_H

#include <math/vector2.h>
#include <math/vector3.h>

#include <string>
#include <vector>

namespace hyperion::v2 {

enum class LoaderFormat {
    NONE,

    OBJ_MODEL,
    OGRE_XML_MODEL,
    OGRE_XML_SKELETON,
    FBOM_MODEL,

    MTL_MATERIAL_LIBRARY,

    TEXTURE_2D,

    WAV_AUDIO,

    SCRIPT_HYPSCRIPT
};

/* Raw data representing a deserialized form of the resource
 * Not the final result - can be a simple struct overloaded to add members
 */
template <class T, LoaderFormat Format>
struct LoaderObject {};

} // namespace hyperion::v2

#endif
