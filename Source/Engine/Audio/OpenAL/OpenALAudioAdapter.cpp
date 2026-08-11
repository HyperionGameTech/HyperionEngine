/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#if defined(HYP_OPENAL) && HYP_OPENAL
#include <Audio/OpenAL/OpenALAudioAdapter.hpp>

#include <Audio/AudioManager.hpp>
#include <Audio/AudioSource.hpp>
#include <Audio/Sound.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <AL/al.h>
#include <AL/alc.h>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Audio);

struct AudioSourceInternalData
{
    ALuint sourceId = 0;
    ALuint bufferId = 0;
};

static ALenum ToALFormat(SoundFormat format)
{
    switch (format)
    {
    case SoundFormat::MONO8:
        return AL_FORMAT_MONO8;
    case SoundFormat::MONO16:
        return AL_FORMAT_MONO16;
    case SoundFormat::STEREO8:
        return AL_FORMAT_STEREO8;
    case SoundFormat::STEREO16:
        return AL_FORMAT_STEREO16;
    default:
        HYP_UNREACHABLE();
    }
}

static const char* ALErrorString(ALenum error)
{
    switch (error)
    {
    case AL_NO_ERROR:
        return "AL_NO_ERROR";
    case AL_INVALID_NAME:
        return "AL_INVALID_NAME";
    case AL_INVALID_ENUM:
        return "AL_INVALID_ENUM";
    case AL_INVALID_VALUE:
        return "AL_INVALID_VALUE";
    case AL_INVALID_OPERATION:
        return "AL_INVALID_OPERATION";
    case AL_OUT_OF_MEMORY:
        return "AL_OUT_OF_MEMORY";
    default:
        return "Unknown AL error";
    }
}

static bool CheckALError(const char* context)
{
    bool hadError = false;

    for (ALenum error = alGetError(); error != AL_NO_ERROR; error = alGetError())
    {
        hadError = true;

        HYP_LOG(Audio, Error, "OpenAL error at '{}': {} ({})", context, ALErrorString(error), int(error));
    }

    return !hadError;
}

OpenALAudioAdapter::OpenALAudioAdapter()
    : m_device(nullptr),
      m_context(nullptr)
{
}

OpenALAudioAdapter::~OpenALAudioAdapter()
{
    Assert(m_device == nullptr);
    Assert(m_context == nullptr);
}

void OpenALAudioAdapter::Init(AudioManager* audioManager)
{
    Assert(m_device == nullptr);
    Assert(m_context == nullptr);

    ALCdevice* device = alcOpenDevice(nullptr);

    if (!device)
    {
        HYP_LOG(Audio, Error, "Failed to open OpenAL device!");

        return;
    }

    ALCcontext* context = alcCreateContext(device, nullptr);

    if (!context)
    {
        HYP_LOG(Audio, Error, "Failed to create OpenAL context!");

        alcCloseDevice(device);

        return;
    }

    if (!alcMakeContextCurrent(context))
    {
        HYP_LOG(Audio, Error, "Failed to make OpenAL context current!");
    }

    m_device = device;
    m_context = context;

    HYP_LOG(Audio, Info, "OpenAL initialized. Device = '{}'", alcGetString(device, ALC_DEVICE_SPECIFIER));

    const ALfloat orientation[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListenerfv(AL_ORIENTATION, orientation);

    CheckALError("Init (listener setup)");
}

void OpenALAudioAdapter::Teardown(AudioManager* audioManager)
{
    if (m_context != nullptr)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(static_cast<ALCcontext*>(m_context));
        m_context = nullptr;
    }

    if (m_device != nullptr)
    {
        alcCloseDevice(static_cast<ALCdevice*>(m_device));
        m_device = nullptr;
    }
}

Array<String> OpenALAudioAdapter::ListDevices() const
{
    Array<String> devices;

    const ALCchar* device = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
    const ALCchar* next = device ? device + 1 : nullptr;

    while (device && *device != '\0' && next && *next != '\0')
    {
        devices.PushBack(device);

        const size_t len = std::strlen(device);
        device += (len + 1);
        next += (len + 2);
    }

    return devices;
}

void OpenALAudioAdapter::SetListenerPosition(const Vec3f& position)
{
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}

void OpenALAudioAdapter::SetListenerOrientation(const Vec3f& forward, const Vec3f& up)
{
    const float values[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, values);
}

void OpenALAudioAdapter::OnAudioSourceInit(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    SharedPtr<AudioSourceInternalData> internalData = MakeShared<AudioSourceInternalData>();

    alGenSources(1, &internalData->sourceId);
    alGenBuffers(1, &internalData->bufferId);

    CheckALError("OnAudioSourceInit (alGenSources/alGenBuffers)");

    HYP_LOG(Audio, Info, "OnAudioSourceInit: sourceId = {}, bufferId = {}", internalData->sourceId, internalData->bufferId);

    audioSource->SetInternalData(std::move(internalData));

    OnAudioSourceSoundChanged(audioSource);
}

void OpenALAudioAdapter::OnAudioSourceDestroy(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourceStop(internalData->sourceId);
    alSourcei(internalData->sourceId, AL_BUFFER, 0);

    alDeleteSources(1, &internalData->sourceId);
    alDeleteBuffers(1, &internalData->bufferId);

    audioSource->SetInternalData(nullptr);
}

void OpenALAudioAdapter::OnAudioSourceSoundChanged(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    const Handle<Sound>& sound = audioSource->GetSound();

    if (!sound.IsValid())
    {
        HYP_LOG(Audio, Info, "OnAudioSourceSoundChanged: no Sound set on AudioSource, detaching buffer");

        alSourcei(internalData->sourceId, AL_BUFFER, 0);

        return;
    }

    auto readScope = sound->GetReadScope();

    const ConstByteView data = sound->GetData();

    HYP_LOG(Audio, Info, "OnAudioSourceSoundChanged: Sound = {}, format = {}, freq = {}, dataSize = {}",
        sound->GetName(), int(sound->GetFormat()), sound->GetFrequency(), data.Size());

    if (data.Size() == 0)
    {
        HYP_LOG(Audio, Warning, "OnAudioSourceSoundChanged: Sound '{}' has no data - was it saved/loaded correctly?", sound->GetName());
    }

    alBufferData(internalData->bufferId, ToALFormat(sound->GetFormat()), data.Data(), ALsizei(data.Size()), ALsizei(sound->GetFrequency()));
    CheckALError("OnAudioSourceSoundChanged (alBufferData)");

    alSourcei(internalData->sourceId, AL_BUFFER, ALint(internalData->bufferId));
    CheckALError("OnAudioSourceSoundChanged (alSourcei AL_BUFFER)");

    ALint boundBuffer = 0;
    alGetSourcei(internalData->sourceId, AL_BUFFER, &boundBuffer);

    ALint bufferSize = 0;
    alGetBufferi(internalData->bufferId, AL_SIZE, &bufferSize);

    HYP_LOG(Audio, Info, "OnAudioSourceSoundChanged: source {} now bound to buffer {} (AL-reported buffer size = {})",
        internalData->sourceId, boundBuffer, bufferSize);

    if (bufferSize == 0)
    {
        HYP_LOG(Audio, Warning, "OnAudioSourceSoundChanged: buffer {} has zero size after alBufferData - source will not produce audio", internalData->bufferId);
    }
}

AudioSourceState OpenALAudioAdapter::GetAudioSourceState(const AudioSource* audioSource) const
{
    Assert(audioSource != nullptr);

    const AudioSourceInternalData* internalData = static_cast<const AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return AudioSourceState::UNDEFINED;
    }

    ALint state;
    alGetSourcei(internalData->sourceId, AL_SOURCE_STATE, &state);

    switch (state)
    {
    case AL_INITIAL: // fallthrough
    case AL_STOPPED:
        return AudioSourceState::STOPPED;
    case AL_PLAYING:
        return AudioSourceState::PLAYING;
    case AL_PAUSED:
        return AudioSourceState::PAUSED;
    default:
        return AudioSourceState::UNDEFINED;
    }
}

void OpenALAudioAdapter::SetAudioSourcePosition(AudioSource* audioSource, const Vec3f& position)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSource3f(internalData->sourceId, AL_POSITION, position.x, position.y, position.z);
}

void OpenALAudioAdapter::SetAudioSourceVelocity(AudioSource* audioSource, const Vec3f& velocity)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSource3f(internalData->sourceId, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

void OpenALAudioAdapter::SetAudioSourcePitch(AudioSource* audioSource, float pitch)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourcef(internalData->sourceId, AL_PITCH, pitch);
}

void OpenALAudioAdapter::SetAudioSourceGain(AudioSource* audioSource, float gain)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourcef(internalData->sourceId, AL_GAIN, gain);
}

void OpenALAudioAdapter::SetAudioSourceLoop(AudioSource* audioSource, bool loop)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourcei(internalData->sourceId, AL_LOOPING, loop);
}

void OpenALAudioAdapter::PlayAudioSource(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    ALint preBuffer = 0;
    alGetSourcei(internalData->sourceId, AL_BUFFER, &preBuffer);

    ALint preState = 0;
    alGetSourcei(internalData->sourceId, AL_SOURCE_STATE, &preState);

    alSourcePlay(internalData->sourceId);
    CheckALError("PlayAudioSource (alSourcePlay)");
}

void OpenALAudioAdapter::PauseAudioSource(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourcePause(internalData->sourceId);
}

void OpenALAudioAdapter::StopAudioSource(AudioSource* audioSource)
{
    Assert(audioSource != nullptr);

    AudioSourceInternalData* internalData = static_cast<AudioSourceInternalData*>(audioSource->GetInternalData());

    if (!internalData)
    {
        return;
    }

    alSourceStop(internalData->sourceId);
}

} // namespace Hyperion

#endif // HYP_OPENAL
