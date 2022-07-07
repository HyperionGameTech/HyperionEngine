#include "WAVAudioLoader.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using AudioLoader = LoaderObject<AudioSource, LoaderFormat::WAV_AUDIO>::Loader;

LoaderResult AudioLoader::LoadFn(LoaderState *state, Object &object)
{
    state->stream.Read(&object.riff_header);

    if (std::strncmp(object.riff_header.chunk_id, "RIFF", 4) != 0) {
        return {LoaderResult::Status::ERR, "invalid RIFF header"};
    }

    if (std::strncmp(object.riff_header.format, "WAVE", 4) != 0) {
        return {LoaderResult::Status::ERR, "invalid WAVE header"};
    }
    
    state->stream.Read(&object.wave_format);

    if (std::strncmp(object.wave_format.sub_chunk_id, "fmt ", 4) != 0) {
        return {LoaderResult::Status::ERR, "invalid wave sub chunk id"};
    }

    if (object.wave_format.sub_chunk_size > 16) {
        state->stream.Skip(sizeof(uint16_t));
    }

    state->stream.Read(&object.wave_data);

    if (std::strncmp(object.wave_data.sub_chunk_id, "data", 4) != 0) {
        return {LoaderResult::Status::ERR, "invalid data header"};
    }
    
    object.wave_bytes.resize(object.wave_data.sub_chunk_2_size);

    state->stream.Read(&object.wave_bytes[0], object.wave_data.sub_chunk_2_size);

    object.size      = object.wave_data.sub_chunk_2_size;
    object.frequency = object.wave_format.sample_rate;

    if (object.wave_format.num_channels == 1) {
        if (object.wave_format.bits_per_sample == 8) {
            object.format = AudioSource::Format::MONO8;
        } else if (object.wave_format.bits_per_sample == 16) {
            object.format = AudioSource::Format::MONO16;
        }
    } else if (object.wave_format.num_channels == 2) {
        if (object.wave_format.bits_per_sample == 8) {
            object.format = AudioSource::Format::STEREO8;
        } else if (object.wave_format.bits_per_sample == 16) {
            object.format = AudioSource::Format::STEREO16;
        }
    }

    return {};
}

std::unique_ptr<AudioSource> AudioLoader::BuildFn(Engine *engine, const Object &object)
{
    return std::make_unique<AudioSource>(
        object.format,
        &object.wave_bytes[0],
        object.wave_bytes.size(),
        object.frequency
    );
}

} // namespace hyperion::v2
