/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/audio_loaders/WAVAudioLoader.hpp>
#include <audio/AudioSource.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

using WAVAudio = WAVAudioLoader::WAVAudio;

AssetLoadResult WAVAudioLoader::LoadAsset(LoaderState& state) const
{
    WAVAudio object;

    state.stream.Read(&object.riffHeader);

    if (std::strncmp(reinterpret_cast<const char*>(object.riffHeader.chunkId), "RIFF", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid RIFF header");
    }

    if (std::strncmp(reinterpret_cast<const char*>(object.riffHeader.format), "WAVE", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid WAVE header");
    }

    state.stream.Read(&object.waveFormat);

    if (std::strncmp(reinterpret_cast<const char*>(object.waveFormat.subChunkId), "fmt ", 4) != 0)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "invalid wave sub chunk id");
    }

    if (object.waveFormat.subChunkSize > 16)
    {
        state.stream.Skip(sizeof(uint16));
    }

    state.stream.Read(&object.waveData);

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

    Handle<AudioSource> audioSource = CreateObject<AudioSource>(
        object.format,
        object.waveBytes,
        object.frequency);

    return LoadedAsset { audioSource };
}

} // namespace hyperion
