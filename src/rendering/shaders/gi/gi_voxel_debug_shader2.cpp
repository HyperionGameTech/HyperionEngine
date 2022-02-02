#include "gi_voxel_debug_shader2.h"
#include "../../environment.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"
#include "../../gi/gi_mapper_camera.h"

namespace hyperion {
    GIVoxelDebugShader2::GIVoxelDebugShader2(const ShaderProperties &properties)
        : LightingShader(properties)
    {
        const std::string vs_path("res/shaders/gi/voxel.vert");
        const std::string gs_path("res/shaders/gi/voxel.geom");
        const std::string fs_path("res/shaders/gi/gi_debug2.frag");

        bool use_geometry_shader = false;

        AddSubShader(
            Shader::SubShaderType::SUBSHADER_VERTEX,
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            ShaderProperties(properties).Define("VCT_GEOMETRY_SHADER", use_geometry_shader),
            vs_path
        );

        AddSubShader(
            Shader::SubShaderType::SUBSHADER_FRAGMENT,
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            ShaderProperties(properties).Define("VCT_GEOMETRY_SHADER", use_geometry_shader),
            fs_path
        );

        if (use_geometry_shader) {
            AddSubShader(
                Shader::SubShaderType::SUBSHADER_GEOMETRY,
                AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
                properties,
                gs_path
            );
        }
    }

    void GIVoxelDebugShader2::ApplyMaterial(const Material &mat)
    {
        LightingShader::ApplyMaterial(mat);

        Environment::GetInstance()->GetGIManager()->GetProbe(0)->GetCamera(0)->Begin();
    }

    void GIVoxelDebugShader2::ApplyTransforms(const Transform &transform, Camera *camera)
    {
        LightingShader::ApplyTransforms(transform, camera);


        Vector3 voxel_bias(20.0f); // todo

        Transform world_to_voxel_tex(Vector3(voxel_bias), Vector3(5.0f)/* scale*/, Quaternion());
        SetUniform("WorldToVoxelTexMatrix", world_to_voxel_tex.GetMatrix());
        

        Matrix4 tmp;

        BoundingBox bb = Environment::GetInstance()->GetGIManager()->GetProbe(0)->GetAABB();
        Vector3 dim = Vector3(0.1f);// bb.GetDimensions();
        //float size = 0.1f;// MathUtil::Max(dim.x, MathUtil::Max(dim.y, dim.z));
        Matrix4 projection;
        MatrixUtil::ToOrtho(projection, -dim.x * 0.5f, dim.x * 0.5f, -dim.y * 0.5f, dim.y * 0.5f, 0.001f, 1.5f);
        MatrixUtil::ToLookAt(tmp, Vector3(bb.GetMax().x, 0, 0) + bb.GetCenter(),
            bb.GetCenter(), Vector3(0, 1, 0));
        Matrix4 mvp_x = projection * tmp;
        MatrixUtil::ToLookAt(tmp, Vector3(0, bb.GetMax().y, 0) + bb.GetCenter(),
            bb.GetCenter(), Vector3(0, 0, -1));
        Matrix4 mvp_y = projection * tmp;
        MatrixUtil::ToLookAt(tmp, Vector3(0, 0, bb.GetMax().z) + bb.GetCenter(),
            bb.GetCenter(), Vector3(0, 1, 0));
        Matrix4 mvp_z = projection * tmp;

        SetUniform("mvp_x", mvp_x);
        SetUniform("mvp_y", mvp_y);
        SetUniform("mvp_z", mvp_z);
    }
} // namespace hyperion
