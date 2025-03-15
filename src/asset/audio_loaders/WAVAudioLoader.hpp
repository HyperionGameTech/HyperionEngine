/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WAV_AUDIO_LOADER_HPP
#define HYPERION_WAV_AUDIO_LOADER_HPP

#include <asset/Assets.hpp>
#include <audio/AudioSource.hpp>

#include <core/memory/ByteBuffer.hpp>

namespace hyperion {

class WAVAudioLoader : public AssetLoader
{
public:

    struct WAVAudio
    {
        struct RiffHeader
        {
            uint8           chunk_id[4];
            uint32          chunk_size;
            uint8           format[4];
        } riff_header;

        struct WaveFormat
        {
            uint8           sub_chunk_id[4];
            uint32          sub_chunk_size;
            uint16          audio_format;
            uint16          num_channels;
            uint32          sample_rate;
            uint32          byte_rate;
            uint16          block_align;
            uint16          bits_per_sample;
        } wave_format;

        struct WaveData
        {
            uint8           sub_chunk_id[4];
            uint32          sub_chunk_2_size;
        } wave_data;

        ByteBuffer          wave_bytes;
        SizeType            size;
        SizeType            frequency;

        AudioSourceFormat   format;
    };

    virtual ~WAVAudioLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif
