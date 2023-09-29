#include "PLYModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

constexpr bool create_obj_indices = true;
constexpr bool mesh_per_material = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool load_materials = true;

using Tokens = std::vector<std::string>;

using PLYModel = PLYModelLoader::PLYModel;

PLYModelLoader::PLYType PLYModelLoader::StringToPLYType(const String &str)
{
    if (str == "float") {
        return PLYType::PLY_TYPE_FLOAT;
    } else if (str == "double") {
        return PLYType::PLY_TYPE_DOUBLE;
    } else if (str == "int") {
        return PLYType::PLY_TYPE_INT;
    } else if (str == "uint") {
        return PLYType::PLY_TYPE_UINT;
    } else if (str == "short") {
        return PLYType::PLY_TYPE_SHORT;
    } else if (str == "ushort") {
        return PLYType::PLY_TYPE_USHORT;
    } else if (str == "char") {
        return PLYType::PLY_TYPE_CHAR;
    } else if (str == "uchar") {
        return PLYType::PLY_TYPE_UCHAR;
    } else {
        return PLYType::PLY_TYPE_UNKNOWN;
    }
}

SizeType PLYModelLoader::PLYTypeSize(PLYType type)
{
    if (type == PLYType::PLY_TYPE_UNKNOWN) {
        return 0;
    }

    static constexpr SizeType type_size_map[PLYType::PLY_TYPE_MAX] = {
        8, // PLY_TYPE_DOUBLE
        4, // PLY_TYPE_FLOAT
        4, // PLY_TYPE_INT
        4, // PLY_TYPE_UINT
        2, // PLY_TYPE_SHORT
        2, // PLY_TYPE_USHORT
        1, // PLY_TYPE_CHAR
        1 // PLY_TYPE_UCHAR
    };

    return type_size_map[type];
}

static void ReadPropertyValue(ByteBuffer &buffer, PLYModelLoader::PLYModel &model, SizeType row_offset, const String &property_name, SizeType count, void *out_ptr);

template<class T>
static void ReadPropertyValue(ByteBuffer &buffer, PLYModelLoader::PLYModel &model, SizeType row_offset, const String &property_name, void *out_ptr) {
    ReadPropertyValue(buffer, model, row_offset, property_name, sizeof(T), out_ptr);
}

static void ReadPropertyValue(ByteBuffer &buffer, PLYModelLoader::PLYModel &model, SizeType row_offset, const String &property_name, SizeType count, void *out_ptr) {
    const auto it = model.property_types.Find(property_name);

    AssertThrowMsg(it != model.property_types.End(), "Property with name %s not found", property_name.Data());
    
    const SizeType offset = it->value.offset + row_offset;
    AssertThrowMsg(offset < buffer.Size(), "Offset out of bounds (%u > %u)", offset, buffer.Size());
    AssertThrowMsg(offset + count <= buffer.Size(), "Offset + Size out of bounds (%u + %llu > %u)", offset, count, buffer.Size());
    
    buffer.Read(offset, count, static_cast<UByte *>(out_ptr));
}

PLYModel PLYModelLoader::LoadModel(LoaderState &state)
{
    PLYModel model;

    SizeType row_length = 0;

    state.stream.ReadLines([&](const String &line, bool *stop_ptr) {
        if (line == "end_header") {
            *stop_ptr = true;

            return;
        }

        const auto split = line.Split(' ');

        if (split[0] == "property") {
            AssertThrowMsg(split.Size() >= 3, "Invalid model header -- property declaration should have at least 3 elements");
            
            const auto property_type_string = split[1];
            const auto property_name = split[2];

            const PLYType property_type = StringToPLYType(property_type_string);

            model.property_types.Set(property_name, PLYPropertyDefinition {
                property_type,
                row_length
            });

            row_length += PLYTypeSize(property_type);
        } else if (split[0] == "element") {
            AssertThrowMsg(split.Size() >= 3, "Invalid model header -- `element` declaration should have at least 3 elements");

            if (split[1] == "vertex") {
                const UInt num_vertices = StringUtil::Parse<UInt>(split[2].Data());

                model.vertices.Resize(num_vertices);
            }
        }
    });
    
    model.header_length = static_cast<UInt>(state.stream.Position());

    const SizeType num_vertices = model.vertices.Size();

    ByteBuffer buffer = state.stream.ReadBytes();

    const auto IsCustomPropertyName = [](const String &str) {
        if (str == "x" || str == "y" || str == "z") {
            return false;
        }

        return true;
    };

    for (auto &it : model.property_types) {
        /*AssertThrowMsg(
            it.value.offset >= model.header_length,
            "Offset out of range of header length (%u >= %u)",
            it.value.offset,
            model.header_length
        );

        it.value.offset -= model.header_length;*/

        DebugLog(LogType::Debug, "%s offset = %u type = %u\n", it.key.Data(), it.value.offset, it.value.type);

        if (IsCustomPropertyName(it.key)) {
            const auto custom_data_it = model.custom_data.Find(it.key);

            if (custom_data_it == model.custom_data.End()) {
                model.custom_data.Set(it.key, ByteBuffer(num_vertices * PLYTypeSize(it.value.type)));
            }
        }
    }
    
    AssertThrow(buffer.Size() + model.header_length == state.stream.Position());

    for (SizeType index = 0; index < num_vertices; index++) {
        const SizeType row_offset = index * row_length;

        Vector3 position(NAN, NAN, NAN);

        ReadPropertyValue<Float>(buffer, model, row_offset, "x", &position.x);
        ReadPropertyValue<Float>(buffer, model, row_offset, "y", &position.y);
        ReadPropertyValue<Float>(buffer, model, row_offset, "z", &position.z);

        Vertex vertex;
        vertex.SetPosition(Vector3(position.x, position.y, position.z));
        model.vertices[index] = vertex;
        
        for (const auto &it : model.property_types) {
            if (!IsCustomPropertyName(it.key)) {
                continue;
            }

            const auto custom_data_it = model.custom_data.Find(it.key);
            AssertThrow(custom_data_it != model.custom_data.End());

            const SizeType data_type_size = PLYTypeSize(it.value.type);
            const SizeType offset = index * data_type_size;

            ReadPropertyValue(
                buffer,
                model,
                row_offset,
                it.key,
                data_type_size,
                custom_data_it->value.Data() + offset
            );
        }
    }

    return model;
}

LoadedAsset PLYModelLoader::BuildModel(LoaderState &state, PLYModel &model)
{
    AssertThrow(state.asset_manager != nullptr);

    auto ply_model_ptr = UniquePtr<PLYModel>::Construct(model);

    return { { LoaderResult::Status::OK }, ply_model_ptr.Cast<void>() };
}

} // namespace hyperion::v2
