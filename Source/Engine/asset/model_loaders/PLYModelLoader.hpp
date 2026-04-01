/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <rendering/Vertex.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class PLYModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(PLYModelLoader);

public:
    enum PLYType
    {
        PLY_TYPE_UNKNOWN = -1,
        PLY_TYPE_DOUBLE,
        PLY_TYPE_FLOAT,
        PLY_TYPE_INT,
        PLY_TYPE_UINT,
        PLY_TYPE_SHORT,
        PLY_TYPE_USHORT,
        PLY_TYPE_CHAR,
        PLY_TYPE_UCHAR,
        PLY_TYPE_MAX
    };

    struct PLYPropertyDefinition
    {
        PLYType type;
        size_t offset;
    };

    struct PLYModel
    {
        HashMap<String, PLYPropertyDefinition> propertyTypes;
        HashMap<String, ByteBuffer> customData;
        Array<SimpleVertex> vertices;
        size_t headerLength = 0;
    };

    static PLYType StringToPLYType(const String& str);

    static size_t PLYTypeSize(PLYType);

    virtual ~PLYModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override
    {
        PLYModel model = LoadModel(state);

        return BuildModel(state, model);
    }

    static PLYModel LoadModel(LoaderState& state);
    static LoadedAsset BuildModel(LoaderState& state, PLYModel& model);
};

} // namespace Hyperion
