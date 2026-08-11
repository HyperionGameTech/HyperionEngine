/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/Assets.hpp>

#include <Core/Memory/ByteBuffer.hpp>

namespace Hyperion {

enum class SoundFormat : uint32;

HYP_CLASS(NoScriptBindings)
class WAVAudioLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(WAVAudioLoader);

public:
    struct WAVAudio
    {
        struct RiffHeader
        {
            uint8 chunkId[4];
            uint32 chunkSize;
            uint8 format[4];
        } riffHeader;

        struct ChunkHeader
        {
            uint8 chunkId[4];
            uint32 chunkSize;
        };

        struct WaveFormat
        {
            uint16 audioFormat;
            uint16 numChannels;
            uint32 sampleRate;
            uint32 byteRate;
            uint16 blockAlign;
            uint16 bitsPerSample;
        } waveFormat;

        ByteBuffer waveBytes;
        size_t size;
        size_t frequency;

        SoundFormat format;
    };

    virtual ~WAVAudioLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
