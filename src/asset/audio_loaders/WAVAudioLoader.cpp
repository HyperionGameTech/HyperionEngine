#include "WAVAudioLoader.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using WAVAudio = WAVAudioLoader::WAVAudio;

LoadedAsset WAVAudioLoader::LoadAsset(LoaderState &state) const
{
    WAVAudio object;

    state.stream.Read(&object.riff_header);

    if (std::strncmp(reinterpret_cast<const char *>(object.riff_header.chunk_id), "RIFF", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid RIFF header"}, UniquePtr<void>() };
    }

    if (std::strncmp(reinterpret_cast<const char *>(object.riff_header.format), "WAVE", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid WAVE header"}, UniquePtr<void>() };
    }
    
    state.stream.Read(&object.wave_format);

    if (std::strncmp(reinterpret_cast<const char *>(object.wave_format.sub_chunk_id), "fmt ", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid wave sub chunk id"}, UniquePtr<void>() };
    }

    if (object.wave_format.sub_chunk_size > 16) {
        state.stream.Skip(sizeof(UInt16));
    }

    state.stream.Read(&object.wave_data);

    if (std::strncmp(reinterpret_cast<const char *>(object.wave_data.sub_chunk_id), "data", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid data header" }, UniquePtr<void>() };
    }
    
    object.wave_bytes.resize(object.wave_data.sub_chunk_2_size);

    state.stream.Read(&object.wave_bytes[0], object.wave_data.sub_chunk_2_size);

    object.size = object.wave_data.sub_chunk_2_size;
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

    auto audio_source = UniquePtr<Handle<AudioSource>>::Construct(
        Engine::Get()->CreateObject<AudioSource>(
            object.format,
            &object.wave_bytes[0],
            object.wave_bytes.size(),
            object.frequency
        )
    );

    return { { LoaderResult::Status::OK }, audio_source.Cast<void>() };
}

} // namespace hyperion::v2
