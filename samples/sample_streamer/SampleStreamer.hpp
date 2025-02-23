#ifndef HYPERION_SAMPLE_STREAMER_HPP
#define HYPERION_SAMPLE_STREAMER_HPP

#include <Game.hpp>
#include <HyperionEngine.hpp>
#include <core/net/MessageQueue.hpp>

#include <rtc/RTCInstance.hpp>

using namespace hyperion;

namespace hyperion {
class AssetBatch;
} // namespace hyperion

class SampleStreamer : public Game
{
public:
    SampleStreamer();
    virtual ~SampleStreamer() = default;

    virtual void Init() override;

    virtual void Teardown() override;
    virtual void Logic(GameCounter::TickUnit delta) override;
    virtual void OnInputEvent(const SystemEvent &event) override;

    virtual void OnFrameEnd(Frame *frame) override;

private:
    Optional<Vec3f> GetWorldRay(const Vec2f &screen_position) const;

    void HandleCompletedAssetBatch(Name, const RC<AssetBatch> &);
    void HandleCameraMovement(GameCounter::TickUnit delta);

    UniquePtr<RTCInstance>              m_rtc_instance;
    RC<RTCStream>                       m_rtc_stream;
    net::MessageQueue                   m_message_queue;

    Handle<Texture>                     m_texture;
    ByteBuffer                          m_screen_buffer;

    FlatMap<Name, RC<AssetBatch>>       m_asset_batches;

    uint32                              m_counter = 0;
};

#endif