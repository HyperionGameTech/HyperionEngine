#include "WavLoader.hpp"
#include "AudioManager.hpp"
#include "AudioSource.hpp"

#include <iostream>
#include <fstream>
namespace hyperion {
#if 0
struct RiffHeader {
    char chunk_id[4];
    long chunk_size;
    char format[4];
};

struct WaveFormat {
    char sub_chunk_id[4];
    long sub_chunk_size;
    short audio_format;
    short num_channels;
    long sample_rate;
    long byte_rate;
    short block_align;
    short bits_per_sample;
};

struct WaveData {
    char sub_chunk_id[4];
    long sub_chunk_2_size;
};

AssetLoader::Result WavLoader::LoadFromFile(const std::string &path)
{
    WaveFormat wave_format;
    RiffHeader riff_header;
    WaveData wave_data;

    unsigned char *data;
    unsigned int buffer;
    size_t size;
    size_t frequency;
    int format;

    std::ifstream fs;
    try {
        fs.open(path, std::ios::binary);

        if (!fs.is_open()) {
            return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, "File could not be opened");
        }

        fs.read((char*)&riff_header, sizeof(RiffHeader));
        if ((riff_header.chunk_id[0] != 'R' ||
            riff_header.chunk_id[1] != 'I' ||
            riff_header.chunk_id[2] != 'F' ||
            riff_header.chunk_id[3] != 'F') ||
            (riff_header.format[0] != 'W' ||
                riff_header.format[1] != 'A' ||
                riff_header.format[2] != 'V' ||
                riff_header.format[3] != 'E')) {
            throw ("Invalid RIFF or WAVE Header");
        }

        fs.read((char*)&wave_format, sizeof(WaveFormat));
        if (wave_format.sub_chunk_id[0] != 'f' ||
            wave_format.sub_chunk_id[1] != 'm' ||
            wave_format.sub_chunk_id[2] != 't' ||
            wave_format.sub_chunk_id[3] != ' ') {
            throw ("Invalid Wave Format");
        }

        if (wave_format.sub_chunk_size > 16) {
            fs.seekg(sizeof(short));
        }


        fs.read((char*)&wave_data, sizeof(WaveData));
        if (wave_data.sub_chunk_id[0] != 'd' ||
            wave_data.sub_chunk_id[1] != 'a' ||
            wave_data.sub_chunk_id[2] != 't' ||
            wave_data.sub_chunk_id[3] != 'a') {
            throw ("Invalid data header");
        }

        // Read data
        data = new unsigned char[wave_data.sub_chunk_2_size];
        fs.read((char*)&data[0], wave_data.sub_chunk_2_size);

        size = wave_data.sub_chunk_2_size;
        frequency = wave_format.sample_rate;

        if (wave_format.num_channels == 1) {
            if (wave_format.bits_per_sample == 8) {
                format = AL_FORMAT_MONO8;
            } else if (wave_format.bits_per_sample == 16) {
                format = AL_FORMAT_MONO16;
            }
        } else if (wave_format.num_channels == 2) {
            if (wave_format.bits_per_sample == 8) {
                format = AL_FORMAT_STEREO8;
            } else if (wave_format.bits_per_sample == 16) {
                format = AL_FORMAT_STEREO16;
            }
        }

        auto audio_source = std::make_shared<AudioSource>(format, data, size, frequency);

        delete[] data;
        fs.close();

        return AssetLoader::Result(audio_source);
    } catch (std::string error) {
        if (fs.is_open()) {
            fs.close();
        }

        return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, error);
    }
}
#endif
} // namespace hyperion
