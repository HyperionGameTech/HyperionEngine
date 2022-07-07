#ifndef HYPERION_V2_WAV_AUDIO_LOADER_H
#define HYPERION_V2_WAV_AUDIO_LOADER_H

#include <asset/LoaderObject.hpp>
#include <asset/Loader.hpp>
#include <audio/AudioSource.hpp>

namespace hyperion::v2 {

template <>
struct LoaderObject<AudioSource, LoaderFormat::WAV_AUDIO> {
    class Loader : public LoaderBase<AudioSource, LoaderFormat::WAV_AUDIO> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<AudioSource> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    struct RiffHeader {
        char chunk_id[4];
        uint32_t chunk_size;
        char format[4];
    } riff_header;

    struct WaveFormat {
        char sub_chunk_id[4];
        uint32_t sub_chunk_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    } wave_format;

    struct WaveData {
        char sub_chunk_id[4];
        uint32_t sub_chunk_2_size;
    } wave_data;

    std::vector<uint8_t> wave_bytes;
    size_t               size;
    size_t               frequency;

    AudioSource::Format format;
};

} // namespace hyperion::v2

#endif
