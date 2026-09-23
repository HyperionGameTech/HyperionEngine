/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/ModelLoaders/PLYModelLoader.hpp>

#include <Rendering/Mesh.hpp>

#include <Scene/Node.hpp>
#include <Scene/Node.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <algorithm>

#include <PLYModelLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

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

size_t PLYModelLoader::PLYTypeSize(PLYType type)
{
    if (type == PLYType::PLY_TYPE_UNKNOWN)
    {
        return 0;
    }

    static constexpr size_t TypeSizeMap[PLYType::PLY_TYPE_MAX] = {
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

static bool ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, size_t rowOffset, const String& propertyName, size_t count, void* outPtr);

template <class T>
static bool ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, size_t rowOffset, const String& propertyName, void* outPtr)
{
    return ReadPropertyValue(buffer, model, rowOffset, propertyName, sizeof(T), outPtr);
}

static bool ReadPropertyValue(ByteBuffer& buffer, PLYModelLoader::PLYModel& model, size_t rowOffset, const String& propertyName, size_t count, void* outPtr)
{
    const auto it = model.propertyTypes.Find(propertyName);

    if (it == model.propertyTypes.End())
    {
        return false;
    }

    const size_t offset = it->second.offset + rowOffset;

    if (offset >= buffer.Size() || count > buffer.Size() - offset)
    {
        return false;
    }

    buffer.Read(offset, count, static_cast<ubyte*>(outPtr));

    return true;
}

PLYModel PLYModelLoader::LoadModel(LoaderState& state)
{
    PLYModel model;

    size_t rowLength = 0;

    String fileContents = String(state.stream.Read().ToByteView());
    Array<String> lines = fileContents.Split('\n');

    for (const String& line : lines)
    {
        if (line == "end_header")
        {
            break;
        }

        const Array<String> split = line.Split(' ');

        if (split.Empty())
        {
            continue;
        }

        if (split[0] == "property")
        {
            if (split.Size() < 3)
            {
                HYP_LOG(Assets, Warning, "Invalid PLY header -- property declaration should have at least 3 elements");

                continue;
            }

            const String propertyTypeString = split[1];
            const String propertyName = split[2];

            const PLYType propertyType = StringToPLYType(propertyTypeString);

            model.propertyTypes.Set(propertyName, PLYPropertyDefinition { propertyType, rowLength });

            rowLength += PLYTypeSize(propertyType);
        }
        else if (split[0] == "element")
        {
            if (split.Size() < 3)
            {
                HYP_LOG(Assets, Warning, "Invalid PLY header -- `element` declaration should have at least 3 elements");

                continue;
            }

            if (split[1] == "vertex")
            {
                const uint32 numVertices = StringUtil::Parse<uint32>(split[2]);

                model.vertices.Resize(numVertices);
            }
        }
    }

    model.headerLength = state.stream.Position();

    const size_t numVertices = model.vertices.Size();

    ByteBuffer buffer = state.stream.Read();

    const auto IsCustomPropertyName = [](const String& str)
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

        if (IsCustomPropertyName(it.first))
        {
            const auto customDataIt = model.customData.Find(it.first);

            if (customDataIt == model.customData.End())
            {
                model.customData.Set(it.first, ByteBuffer(numVertices * PLYTypeSize(it.second.type)));
            }
        }
    }

    Assert(buffer.Size() + model.headerLength == state.stream.Position());

    for (size_t index = 0; index < numVertices; index++)
    {
        const size_t rowOffset = index * rowLength;

        Vector3 position(NAN, NAN, NAN);

        if (!ReadPropertyValue<float>(buffer, model, rowOffset, "x", &position.x)
            || !ReadPropertyValue<float>(buffer, model, rowOffset, "y", &position.y)
            || !ReadPropertyValue<float>(buffer, model, rowOffset, "z", &position.z))
        {
            HYP_LOG(Assets, Warning, "PLY vertex {} is missing position data or is out of bounds", index);

            model.vertices.Clear();

            break;
        }

        SimpleVertex vertex {};
        vertex.SetPosition(Vector3(position.x, position.y, position.z));
        model.vertices[index] = vertex;

        for (const auto& it : model.propertyTypes)
        {
            if (!IsCustomPropertyName(it.first))
            {
                continue;
            }

            const auto customDataIt = model.customData.Find(it.first);
            Assert(customDataIt != model.customData.End());

            const size_t dataTypeSize = PLYTypeSize(it.second.type);
            const size_t offset = index * dataTypeSize;

            if (!ReadPropertyValue(
                    buffer,
                    model,
                    rowOffset,
                    it.first,
                    dataTypeSize,
                    customDataIt->second.Data() + offset))
            {
                HYP_LOG(Assets, Warning, "PLY property '{}' out of bounds for vertex {}", it.first, index);
            }
        }
    }

    return model;
}

LoadedAsset PLYModelLoader::BuildModel(LoaderState& state, PLYModel& model)
{
    Assert(state.assetManager != nullptr);

    SharedPtr<PLYModel> plyModelPtr = MakeShared<PLYModel>(model);

    return LoadedAsset { plyModelPtr };
}

} // namespace Hyperion
