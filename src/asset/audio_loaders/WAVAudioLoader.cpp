/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/audio_loaders/WAVAudioLoader.hpp>
#include <Engine.hpp>

namespace hyperion {

using WAVAudio = WAVAudioLoader::WAVAudio;

LoadedAsset WAVAudioLoader::LoadAsset(LoaderState &state) const
{
    WAVAudio object;

    state.stream.Read(&object.riff_header);

    if (std::strncmp(reinterpret_cast<const char *>(object.riff_header.chunk_id), "RIFF", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid RIFF header"} };
    }

    if (std::strncmp(reinterpret_cast<const char *>(object.riff_header.format), "WAVE", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid WAVE header"} };
    }
    
    state.stream.Read(&object.wave_format);

    if (std::strncmp(reinterpret_cast<const char *>(object.wave_format.sub_chunk_id), "fmt ", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid wave sub chunk id"} };
    }

    if (object.wave_format.sub_chunk_size > 16) {
        state.stream.Skip(sizeof(uint16));
    }

    state.stream.Read(&object.wave_data);

    if (std::strncmp(reinterpret_cast<const char *>(object.wave_data.sub_chunk_id), "data", 4) != 0) {
        return { { LoaderResult::Status::ERR, "invalid data header" } };
    }
    
    object.wave_bytes.SetSize(object.wave_data.sub_chunk_2_size);

    state.stream.Read(object.wave_bytes.Data(), object.wave_data.sub_chunk_2_size);

    object.size = object.wave_data.sub_chunk_2_size;
    object.frequency = object.wave_format.sample_rate;

    if (object.wave_format.num_channels == 1) {
        if (object.wave_format.bits_per_sample == 8) {
            object.format = AudioSourceFormat::MONO8;
        } else if (object.wave_format.bits_per_sample == 16) {
            object.format = AudioSourceFormat::MONO16;
        }
    } else if (object.wave_format.num_channels == 2) {
        if (object.wave_format.bits_per_sample == 8) {
            object.format = AudioSourceFormat::STEREO8;
        } else if (object.wave_format.bits_per_sample == 16) {
            object.format = AudioSourceFormat::STEREO16;
        }
    }

    ByteBuffer byte_buffer(object.wave_bytes.Size(), &object.wave_bytes[0]);

    Handle<AudioSource> audio_source = CreateObject<AudioSource>(
        object.format,
        byte_buffer,
        object.frequency
    );

    return { { LoaderResult::Status::OK }, audio_source };
}

} // namespace hyperion
