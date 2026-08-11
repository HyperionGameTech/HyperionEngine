/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/AudioLoaders/WAVAudioLoader.hpp>
#include <Audio/Sound.hpp>

#include <Core/Math/MathUtil.hpp>

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

    bool haveFormat = false;
    bool haveData = false;

    while (!state.stream.Eof() && (!haveFormat || !haveData))
    {
        WAVAudio::ChunkHeader chunkHeader;
        state.stream.Read(&chunkHeader, sizeof(chunkHeader));

        if (std::strncmp(reinterpret_cast<const char*>(chunkHeader.chunkId), "fmt ", 4) == 0)
        {
            const uint32 bytesToRead = MathUtil::Min<uint32>(chunkHeader.chunkSize, sizeof(WAVAudio::WaveFormat));

            object.waveFormat = {};
            state.stream.Read(&object.waveFormat, bytesToRead);

            if (chunkHeader.chunkSize > bytesToRead)
            {
                state.stream.Skip(chunkHeader.chunkSize - bytesToRead);
            }

            haveFormat = true;
        }
        else if (std::strncmp(reinterpret_cast<const char*>(chunkHeader.chunkId), "data", 4) == 0)
        {
            object.waveBytes.SetSize(chunkHeader.chunkSize);
            state.stream.Read(object.waveBytes.Data(), chunkHeader.chunkSize);

            haveData = true;
        }
        else
        {
            state.stream.Skip(chunkHeader.chunkSize);
        }

        // chunks are padded to an even number of bytes
        if (chunkHeader.chunkSize & 1)
        {
            state.stream.Skip(1);
        }
    }

    if (!haveFormat)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "missing fmt chunk");
    }

    if (!haveData)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "missing data chunk");
    }

    // SoundFormat / OpenAL only understand 8 and 16 bit PCM, so anything wider needs
    // to be squashed down to 16 bit before it reaches the audio backend.
    if (object.waveFormat.bitsPerSample == 24)
    {
        const uint8* src = object.waveBytes.Data();
        const size_t numSamples = object.waveBytes.Size() / 3;

        ByteBuffer converted;
        converted.SetSize(numSamples * sizeof(int16));

        int16* dst = reinterpret_cast<int16*>(converted.Data());

        for (size_t i = 0; i < numSamples; i++)
        {
            const int32 sample = (int32(src[i * 3 + 0]) << 8)
                | (int32(src[i * 3 + 1]) << 16)
                | (int32(src[i * 3 + 2]) << 24);

            dst[i] = int16(sample >> 16);
        }

        object.waveBytes = std::move(converted);
        object.waveFormat.bitsPerSample = 16;
    }

    object.size = object.waveBytes.Size();
    object.frequency = object.waveFormat.sampleRate;

    if (object.waveFormat.numChannels == 1)
    {
        if (object.waveFormat.bitsPerSample == 8)
        {
            object.format = SoundFormat::MONO8;
        }
        else if (object.waveFormat.bitsPerSample == 16)
        {
            object.format = SoundFormat::MONO16;
        }
        else
        {
            return HYP_MAKE_ERROR(AssetLoadError, "unsupported bits per sample");
        }
    }
    else if (object.waveFormat.numChannels == 2)
    {
        if (object.waveFormat.bitsPerSample == 8)
        {
            object.format = SoundFormat::STEREO8;
        }
        else if (object.waveFormat.bitsPerSample == 16)
        {
            object.format = SoundFormat::STEREO16;
        }
        else
        {
            return HYP_MAKE_ERROR(AssetLoadError, "unsupported bits per sample");
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "unsupported channel count");
    }

    Name assetName = CreateNameFromDynamicString(StringUtil::StripExtension(state.filepath.Basename()));

    Handle<Sound> sound = MakeHandle<Sound>();
    sound->SetName(assetName);
    sound->SetFormat(object.format);
    sound->SetFrequency(object.frequency);
    sound->SetData(object.waveBytes.ToByteView());

    return LoadedAsset { sound };
}

} // namespace Hyperion
