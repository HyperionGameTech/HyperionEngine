/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/audio_loaders/WAVAudioLoader.hpp>
#include <Audio/AudioSource.hpp>

#include <Framework/EngineDriver.hpp>

#include <WAVAudioLoader.generated.inl>

namespace Hyperion {

using WAVAudio = WAVAudioLoader::WAVAudio;

AssetLoadResult WAVAudioLoader::LoadAsset(LoaderState& state) const
{
    WAVAudio object;

    state.stream.Read(&object.riffHeader, sizeof(WAVAudio::RiffHeader));

    if (std::strncmp(reinterpret_cast<const char*>(object.riffHeader.chunkId), "RIFF", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid RIFF header");
    }

    if (std::strncmp(reinterpret_cast<const char*>(object.riffHeader.format), "WAVE", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid WAVE header");
    }

    state.stream.Read(&object.waveFormat, sizeof(WAVAudio::WaveFormat));

    if (std::strncmp(reinterpret_cast<const char*>(object.waveFormat.subChunkId), "fmt ", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid wave sub chunk id");
    }

    if (object.waveFormat.subChunkSize > 16)
    {
        state.stream.Skip(sizeof(uint16));
    }

    state.stream.Read(&object.waveData, sizeof(WAVAudio::WaveData));

    if (std::strncmp(reinterpret_cast<const char*>(object.waveData.subChunkId), "data", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid data header");
    }

    object.waveBytes.SetSize(object.waveData.subChunk2Size);

    state.stream.Read(object.waveBytes.Data(), object.waveData.subChunk2Size);

    object.size = object.waveData.subChunk2Size;
    object.frequency = object.waveFormat.sampleRate;

    if (object.waveFormat.numChannels == 1)
    {
        if (object.waveFormat.bitsPerSample == 8)
        {
            object.format = AudioSourceFormat::MONO8;
        }
        else if (object.waveFormat.bitsPerSample == 16)
        {
            object.format = AudioSourceFormat::MONO16;
        }
    }
    else if (object.waveFormat.numChannels == 2)
    {
        if (object.waveFormat.bitsPerSample == 8)
        {
            object.format = AudioSourceFormat::STEREO8;
        }
        else if (object.waveFormat.bitsPerSample == 16)
        {
            object.format = AudioSourceFormat::STEREO16;
        }
    }

    Handle<AudioSource> audioSource = MakeHandle<AudioSource>(
        object.format,
        object.waveBytes,
        object.frequency);

    return LoadedAsset { audioSource };
}

} // namespace Hyperion
