/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>

namespace Hyperion {

HYP_ENUM()
enum class SoundFormat : uint32
{
    MONO8,
    MONO16,
    STEREO8,
    STEREO16
};

HYP_CLASS(AssetBucket = "Sounds")
class ENGINE_API Sound : public AssetObject
{
    HYP_OBJECT_BODY(Sound);

public:
    Sound()
        : AssetObject()
    {
    }

    explicit Sound(Name name)
        : AssetObject(name)
    {
    }

    Sound(const Sound& other) = delete;
    Sound& operator=(const Sound& other) = delete;

    Sound(Sound&& other) noexcept = delete;
    Sound& operator=(Sound&& other) noexcept = delete;

    ~Sound();

    HYP_METHOD(Property = "Format", Serialize)
    HYP_FORCE_INLINE SoundFormat GetFormat() const
    {
        return m_format;
    }

    HYP_METHOD(Property = "Format", Serialize)
    void SetFormat(SoundFormat format);

    HYP_METHOD(Property = "Frequency", Serialize)
    HYP_FORCE_INLINE uint64 GetFrequency() const
    {
        return m_freq;
    }

    HYP_METHOD(Property = "Frequency", Serialize)
    void SetFrequency(uint64 freq);

    HYP_FORCE_INLINE uint32 GetSampleLength() const
    {
        return m_sampleLength;
    }

    /*! \brief Get duration in seconds. */
    HYP_METHOD()
    HYP_FORCE_INLINE float GetDuration() const
    {
        return static_cast<float>(m_sampleLength) / static_cast<float>(m_freq);
    }

    void SetData(ConstByteView view);
    ConstByteView GetData() const;

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("SND", 1, &m_data);
    }

private:
    void RecomputeSampleLength();

    HYP_FIELD(Property = "Format", Serialize)
    SoundFormat m_format = SoundFormat::MONO8;

    HYP_FIELD(Property = "Frequency", Serialize)
    uint64 m_freq = 0;

    HYP_FIELD(Property = "SampleLength", Serialize)
    uint32 m_sampleLength = 0;

    HYP_FIELD(Property = "Data", Serialize)
    BlobDataReference m_data;
};

} // namespace Hyperion
