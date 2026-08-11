/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Audio/Sound.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Logging/Logger.hpp>

#include <Sound.generated.inl>

namespace Hyperion {

Sound::~Sound()
{
    FreeBlobData(m_data);
}

static void GetFormatInfo(SoundFormat format, uint32& outNumChannels, uint32& outBitsPerSample)
{
    switch (format)
    {
    case SoundFormat::MONO8:
        outNumChannels = 1;
        outBitsPerSample = 8;
        break;
    case SoundFormat::MONO16:
        outNumChannels = 1;
        outBitsPerSample = 16;
        break;
    case SoundFormat::STEREO8:
        outNumChannels = 2;
        outBitsPerSample = 8;
        break;
    case SoundFormat::STEREO16:
        outNumChannels = 2;
        outBitsPerSample = 16;
        break;
    default:
        HYP_UNREACHABLE();
    }
}

void Sound::SetFormat(SoundFormat format)
{
    if (m_format == format)
    {
        return;
    }

    m_format = format;

    RecomputeSampleLength();

    MarkDirty();
}

void Sound::SetFrequency(uint64 freq)
{
    if (m_freq == freq)
    {
        return;
    }

    m_freq = freq;

    RecomputeSampleLength();

    MarkDirty();
}

void Sound::SetData(ConstByteView view)
{
    if (view.Size() == 0 && m_data.size == 0)
    {
        return;
    }

    FreeBlobData(m_data);

    if (view.Size() != 0)
    {
        AllocateBlobData(m_data, view.Data(), view.Size(), 1);
    }

    RecomputeSampleLength();

    MarkDirty();
}

void Sound::RecomputeSampleLength()
{
    if (m_data.size == 0 || m_freq == 0)
    {
        m_sampleLength = 0;

        return;
    }

    uint32 numChannels;
    uint32 bitsPerSample;
    GetFormatInfo(m_format, numChannels, bitsPerSample);

    m_sampleLength = uint32((m_data.size * 8) / (numChannels * bitsPerSample));
}

ConstByteView Sound::GetData() const
{
    if (m_data.raw == nullptr || m_data.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView(reinterpret_cast<const ubyte*>(m_data.raw), m_data.size);
}

void Sound::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    if (m_data.raw == nullptr
        && m_data.key
        && m_data.size != 0)
    {
        if (PageBlobDataFromStorage(m_data))
        {
            return;
        }

        const Name blobKey = m_data.key;
        const uint64 expectedSize = m_data.size;

        FileByteReader stream { registry->GetRootPath() / AssetBuckets::Sounds.GetName() / (String(*GetName()) + ".SND.raw.blob") };

        if (!stream.Eof())
        {
            if (stream.Max() != expectedSize)
            {
                HYP_LOG(Assets, Error, "Local blob data for Sound asset '{}' is {} bytes but the manifest expects {}, ignoring it",
                        GetName(), stream.Max(), expectedSize);

                return;
            }

            ByteBuffer buffer = stream.Read(stream.Max());

            AllocateBlobData(m_data, buffer.Data(), buffer.Size(), 1);
            m_data.key = blobKey;

            return;
        }

        m_data.readOnly = true;
    }
}

void Sound::UnpageBlobData()
{
    if (m_data.readOnly)
    {
        m_data.raw = nullptr;
    }
}

} // namespace Hyperion
