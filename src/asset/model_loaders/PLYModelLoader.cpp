/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/PLYModelLoader.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <algorithm>

namespace hyperion {

constexpr bool create_obj_indices = true;
constexpr bool mesh_per_material = true; // set true to create a new mesh on each instance of 'use <mtllib>'
constexpr bool load_materials = true;

using PLYModel = PLYModelLoader::PLYModel;

PLYModelLoader::PLYType PLYModelLoader::StringToPLYType(const String &str)
{
    if (str == "float") {
        return PLYType::PLY_TYPE_FLOAT;
    } else if (str == "double") {
        return PLYType::PLY_TYPE_DOUBLE;
    } else if (str == "int") {
        return PLYType::PLY_TYPE_INT;
    } else if (str == "uint32") {
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
    
    const SizeType offset = it->second.offset + row_offset;
    AssertThrowMsg(offset < buffer.Size(), "Offset out of bounds (%u > %u)", offset, buffer.Size());
    AssertThrowMsg(offset + count <= buffer.Size(), "Offset + Size out of bounds (%u + %llu > %u)", offset, count, buffer.Size());
    
    buffer.Read(offset, count, static_cast<ubyte *>(out_ptr));
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

        if (split.Empty()) {
            return;
        }

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
                const uint32 num_vertices = StringUtil::Parse<uint32>(split[2].Data());

                model.vertices.Resize(num_vertices);
            }
        }
    });
    
    model.header_length = state.stream.Position();

    const SizeType num_vertices = model.vertices.Size();

    ByteBuffer buffer = state.stream.ReadBytes();

    const auto IsCustomPropertyName = [](const String &str)
    {
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

        DebugLog(LogType::Debug, "%s offset = %u type = %u\n", it.first.Data(), it.second.offset, it.second.type);

        if (IsCustomPropertyName(it.first)) {
            const auto custom_data_it = model.custom_data.Find(it.first);

            if (custom_data_it == model.custom_data.End()) {
                model.custom_data.Set(it.first, ByteBuffer(num_vertices * PLYTypeSize(it.second.type)));
            }
        }
    }
    
    AssertThrow(buffer.Size() + model.header_length == state.stream.Position());

    for (SizeType index = 0; index < num_vertices; index++) {
        const SizeType row_offset = index * row_length;

        Vector3 position(NAN, NAN, NAN);

        ReadPropertyValue<float>(buffer, model, row_offset, "x", &position.x);
        ReadPropertyValue<float>(buffer, model, row_offset, "y", &position.y);
        ReadPropertyValue<float>(buffer, model, row_offset, "z", &position.z);

        Vertex vertex;
        vertex.SetPosition(Vector3(position.x, position.y, position.z));
        model.vertices[index] = vertex;
        
        for (const auto &it : model.property_types) {
            if (!IsCustomPropertyName(it.first)) {
                continue;
            }

            const auto custom_data_it = model.custom_data.Find(it.first);
            AssertThrow(custom_data_it != model.custom_data.End());

            const SizeType data_type_size = PLYTypeSize(it.second.type);
            const SizeType offset = index * data_type_size;

            ReadPropertyValue(
                buffer,
                model,
                row_offset,
                it.first,
                data_type_size,
                custom_data_it->second.Data() + offset
            );
        }
    }

    return model;
}

LoadedAsset PLYModelLoader::BuildModel(LoaderState &state, PLYModel &model)
{
    AssertThrow(state.asset_manager != nullptr);

    RC<PLYModel> ply_model_ptr = MakeRefCountedPtr<PLYModel>(model);

    return { { LoaderResult::Status::OK }, ply_model_ptr };
}

} // namespace hyperion
