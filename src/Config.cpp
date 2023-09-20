#include <Config.hpp>
#include <Engine.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <asset/ByteWriter.hpp>

namespace hyperion::v2 {

const FlatMap<OptionName, String> Configuration::option_name_strings = {
    { CONFIG_DEBUG_MODE, "DebugMode" },
    { CONFIG_SHADER_COMPILATION, "ShaderCompilation" },
    { CONFIG_RT_SUPPORTED, "RTSupported" },
    { CONFIG_RT_ENABLED, "RTEnabled" },
    { CONFIG_RT_REFLECTIONS, "RTReflections" },
    { CONFIG_RT_GI, "RTGlobalIllumination" },
    { CONFIG_RT_GI_DEBUG_PROBES, "DebugDDGIProbes" },
    { CONFIG_PATHTRACER,  "PathTracer" },
    { CONFIG_SSR, "ScreenSpaceReflections" },
    { CONFIG_ENV_GRID_REFLECTIONS, "EnvGridReflections" },
    { CONFIG_ENV_GRID_GI, "EnvGridGlobalIllumination" },
    { CONFIG_HBAO, "HBAO" },
    { CONFIG_HBIL, "HBIL" },
    { CONFIG_VOXEL_GI, "VCTGlobalIllumination" },
    { CONFIG_VOXEL_GI_SVO, "VCTGlobalIlluminationSVO" },
    { CONFIG_TEMPORAL_AA, "TemporalAA" },
    { CONFIG_LIGHT_RAYS, "LightRays" },
    { CONFIG_DEBUG_SSR, "DebugSSR" },
    { CONFIG_DEBUG_HBAO, "DebugHBAO" },
    { CONFIG_DEBUG_HBIL, "DebugHBIL" },
    { CONFIG_DEBUG_REFLECTIONS, "DebugReflections" },
    { CONFIG_DEBUG_IRRADIANCE, "DebugIrradiance" },
    { CONFIG_DEBUG_ENV_GRID_PROBES, "DebugEnvGridProbes" }
};

OptionName Configuration::StringToOptionName(const String &str)
{
    for (auto &it : option_name_strings) {
        if (it.second == str) {
            return it.first;
        }
    }

    return CONFIG_NONE;
}

String Configuration::OptionNameToString(OptionName option)
{
    const auto it = option_name_strings.Find(option);

    if (it == option_name_strings.End()) {
        return "Unknown";
    }

    return it->second;
}

bool Configuration::IsRTOption(OptionName option)
{
    return option == CONFIG_RT_SUPPORTED
        || option == CONFIG_RT_ENABLED
        || option == CONFIG_RT_REFLECTIONS
        || option == CONFIG_RT_GI
        || option == CONFIG_RT_GI_DEBUG_PROBES
        || option == CONFIG_PATHTRACER;
}

Configuration::Configuration() = default;

bool Configuration::LoadFromDefinitionsFile()
{
    const DefinitionsFile definitions(g_asset_manager->GetBasePath() / "config.def");

    if (!definitions.IsValid()) {
        return false;
    }

    for (const auto &it : definitions.GetSections()) {
        for (const auto &option_it : it.value) {
            const OptionName option_name = StringToOptionName(option_it.key);
            const DefinitionsFile::Value &option_value = option_it.value;

            if (option_name == CONFIG_NONE || option_name >= CONFIG_MAX) {
                DebugLog(
                    LogType::Warn,
                    "%s: Unknown config option\n",
                    option_it.key.Data()
                );

                continue;
            }

            Option value;

            if (IsRTOption(option_name) && !g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()) {
                value = false;
            } else {
                union { Int i; Float f; } tmp_value;

                if (option_value.GetValue().name == "true") {
                    value = true;
                } else if (option_value.GetValue().name == "false") {
                    value = false;
                } else if (StringUtil::Parse(option_it.key.Data(), &tmp_value.i)) {
                    value = tmp_value.i;
                } else if (StringUtil::Parse(option_it.key.Data(), &tmp_value.f)) {
                    value = tmp_value.f;
                } else {
                    value = false;
                }
            }

            m_variables[option_name] = value;
        }
    }

    return true;
}

bool Configuration::SaveToDefinitionsFile()
{
    String str_result = "[Default]\n";

    for (UInt index = CONFIG_NONE + 1; index < CONFIG_MAX; index++) {
        const Option &option = m_variables[index];

        if (!option.GetIsSaved()) {
            continue;
        }

        String value_string = "false";

        if (option.IsValid()) {
            if (option.Is<bool>()) {
                value_string = option.GetBool() ? "true" : "false";
            } else if (option.Is<Int>()) {
                value_string = String::ToString(option.GetInt());
            } else if (option.Is<Float>()) {
                // TODO!
                //value_string = String::ToString(option.GetFloat());
            }
        }

        str_result += OptionNameToString(OptionName(index)) + " = " + value_string + "\n";
    }

    const String path = g_asset_manager->GetBasePath() / "config.def";

    FileByteWriter writer(path.Data());

    if (!writer.IsOpen()) {
        return false;
    }

    writer.Write(str_result.Data(), str_result.Size());
    writer.Close();

    return true;
}

void Configuration::SetToDefaultConfiguration()
{
    m_variables = { };

#ifdef HYP_DEBUG_MODE
    m_variables[CONFIG_DEBUG_MODE] = Option(true, false);
#endif

    m_variables[CONFIG_SHADER_COMPILATION] = Option(bool(HYP_GLSLANG), false);
    
    m_variables[CONFIG_RT_SUPPORTED] = Option(g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported(), false);
    m_variables[CONFIG_RT_ENABLED] = Option(m_variables[CONFIG_RT_SUPPORTED] && g_engine->GetGPUDevice()->GetFeatures().IsRaytracingEnabled(), true);
    m_variables[CONFIG_RT_REFLECTIONS] = Option(m_variables[CONFIG_RT_ENABLED], true);
    m_variables[CONFIG_RT_GI] = Option(m_variables[CONFIG_RT_ENABLED], true);

    m_variables[CONFIG_HBAO] = Option(true, true);
    m_variables[CONFIG_HBIL] = Option(m_variables[CONFIG_HBAO], true);
    
    m_variables[CONFIG_SSR] = Option(!m_variables[CONFIG_RT_REFLECTIONS], true);

    m_variables[CONFIG_VOXEL_GI] = Option(false, true);
    m_variables[CONFIG_VOXEL_GI_SVO] = Option(false, true);

    m_variables[CONFIG_TEMPORAL_AA] = Option(true, true);

    m_variables[CONFIG_LIGHT_RAYS] = Option(true, true);

    m_variables[CONFIG_PATHTRACER] = Option(false, true);

    m_variables[CONFIG_DEBUG_SSR] = Option(false, true);
    m_variables[CONFIG_DEBUG_HBAO] = Option(false, true);
    m_variables[CONFIG_DEBUG_HBIL] = Option(false, true);
    m_variables[CONFIG_DEBUG_REFLECTIONS] = Option(false, true);
    m_variables[CONFIG_DEBUG_IRRADIANCE] = Option(false, true);
    m_variables[CONFIG_DEBUG_ENV_GRID_PROBES] = Option(false, true);
}

} // namespace hyperion::v2