/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/model_loaders/PLYModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/Node.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <algorithm>

#include <PLYModelLoader.generated.inl>

namespace hyperion {

constexpr bool ShouldCreateIndices = true;
constexpr bool ShouldCreateNewMeshPerMaterial = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool ShouldLoadMaterials = true;

using PLYModel = PLYModelLoader::PLYModel;

PLYModelLoader::PLYType PLYModelLoader::StringToPLYType(const String& str)
{
    if (str == "float")
    {
        return PLYType::PLY_TYPE_FLOAT;
    }
    else if (str == "double")
    {
        return PLYType::PLY_TYPE_DOUBLE;
    }
    else if (str == "int")
    {
        return PLYType::PLY_TYPE_INT;
    }
    else if (str == "uint32")
    {
        return PLYType::PLY_TYPE_UINT;
    }
    else if (str == "short")
    {
        return PLYType::PLY_TYPE_SHORT;
    }
    else if (str == "ushort")
    {
        return PLYType::PLY_TYPE_USHORT;
    }
    else if (str == "char")
    {
        return PLYType::PLY_TYPE_CHAR;
    }
    else if (str == "uchar")
    {
        return PLYType::PLY_TYPE_UCHAR;
    }
    else
    {
        return PLYType::PLY_TYPE_UNKNOWN;
    }
}

SizeType PLYModelLoader::PLYTypeSize(PLYType type)
{
    if (type == PLYType::PLY_TYPE_UNKNOWN)
    {
        return 0;
    }

    static constexpr SizeType TypeSizeMap[PLYType::PLY_TYPE_MAX] = {
        8, // PLY_TYPE_DOUBLE
        4, // PLY_TYPE_FLOAT
        4, // PLY_TYPE_INT
        4, // PLY_TYPE_UINT
        2, // PLY_TYPE_SHORT
        2, // PLY_TYPE_USHORT
        1, // PLY_TYPE_CHAR
        1  // PLY_TYPE_UCHAR
    };

    return TypeSizeMap[type];
}

static void ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, SizeType rowOffset, const String& propertyName, SizeType count, void* outPtr);

template <class T>
static void ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, SizeType rowOffset, const String& propertyName, void* outPtr)
{
    ReadPropertyValue(buffer, model, rowOffset, propertyName, sizeof(T), outPtr);
}

static void ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, SizeType rowOffset, const String& propertyName, SizeType count, void* outPtr)
{
    const auto it = model.propertyTypes.Find(propertyName);

    Assert(it != model.propertyTypes.End(), "Property with name %s not found", propertyName.Data());

    const SizeType offset = it->second.offset + rowOffset;
    Assert(offset < buffer.Size(), "Offset out of bounds (%u > %u)", offset, buffer.Size());
    Assert(offset + count <= buffer.Size(), "Offset + Size out of bounds (%u + %llu > %u)", offset, count, buffer.Size());

    buffer.Read(offset, count, static_cast<ubyte*>(outPtr));
}

PLYModel PLYModelLoader::LoadModel(LoaderState& state)
{
    PLYModel model;

    SizeType rowLength = 0;

    state.stream.ReadLines([&](const String& line, bool* stopPtr)
        {
            if (line == "end_header")
            {
                *stopPtr = true;

                return;
            }

            const auto split = line.Split(' ');

            if (split.Empty())
            {
                return;
            }

            if (split[0] == "property")
            {
                Assert(split.Size() >= 3, "Invalid model header -- property declaration should have at least 3 elements");

                const auto propertyTypeString = split[1];
                const auto propertyName = split[2];

                const PLYType propertyType = StringToPLYType(propertyTypeString);

                model.propertyTypes.Set(propertyName, PLYPropertyDefinition { propertyType, rowLength });

                rowLength += PLYTypeSize(propertyType);
            }
            else if (split[0] == "element")
            {
                Assert(split.Size() >= 3, "Invalid model header -- `element` declaration should have at least 3 elements");

                if (split[1] == "vertex")
                {
                    const uint32 numVertices = StringUtil::Parse<uint32>(split[2].Data());

                    model.vertices.Resize(numVertices);
                }
            }
        });

    model.headerLength = state.stream.Position();

    const SizeType numVertices = model.vertices.Size();

    ByteBuffer buffer = state.stream.ReadBytes();

    const auto isCustomPropertyName = [](const String& str)
    {
        if (str == "x" || str == "y" || str == "z")
        {
            return false;
        }

        return true;
    };

    for (auto& it : model.propertyTypes)
    {
        /*Assert(
            it.value.offset >= model.headerLength,
            "Offset out of range of header length (%u >= %u)",
            it.value.offset,
            model.headerLength
        );

        it.value.offset -= model.headerLength;*/

        DebugLog(LogType::Debug, "%s offset = %u type = %u\n", it.first.Data(), it.second.offset, it.second.type);

        if (isCustomPropertyName(it.first))
        {
            const auto customDataIt = model.customData.Find(it.first);

            if (customDataIt == model.customData.End())
            {
                model.customData.Set(it.first, ByteBuffer(numVertices * PLYTypeSize(it.second.type)));
            }
        }
    }

    Assert(buffer.Size() + model.headerLength == state.stream.Position());

    for (SizeType index = 0; index < numVertices; index++)
    {
        const SizeType rowOffset = index * rowLength;

        Vector3 position(NAN, NAN, NAN);

        ReadPropertyValue<float>(buffer, model, rowOffset, "x", &position.x);
        ReadPropertyValue<float>(buffer, model, rowOffset, "y", &position.y);
        ReadPropertyValue<float>(buffer, model, rowOffset, "z", &position.z);

        Vertex vertex;
        vertex.SetPosition(Vector3(position.x, position.y, position.z));
        model.vertices[index] = vertex;

        for (const auto& it : model.propertyTypes)
        {
            if (!isCustomPropertyName(it.first))
            {
                continue;
            }

            const auto customDataIt = model.customData.Find(it.first);
            Assert(customDataIt != model.customData.End());

            const SizeType dataTypeSize = PLYTypeSize(it.second.type);
            const SizeType offset = index * dataTypeSize;

            ReadPropertyValue(
                buffer,
                model,
                rowOffset,
                it.first,
                dataTypeSize,
                customDataIt->second.Data() + offset);
        }
    }

    return model;
}

LoadedAsset PLYModelLoader::BuildModel(LoaderState& state, PLYModel& model)
{
    Assert(state.assetManager != nullptr);

    RC<PLYModel> plyModelPtr = MakeRefCountedPtr<PLYModel>(model);

    return LoadedAsset { plyModelPtr };
}

} // namespace hyperion
