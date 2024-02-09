#ifndef HYPERION_SAMPLE_STREAMER_HPP
#define HYPERION_SAMPLE_STREAMER_HPP

#include <Game.hpp>
#include <HyperionEngine.hpp>
#include <core/net/MessageQueue.hpp>

#include <rtc/RTCInstance.hpp>

using namespace hyperion;
using namespace hyperion::v2;

class SampleStreamer : public Game
{
public:
    SampleStreamer(RC<Application> application);
    virtual ~SampleStreamer() = default;

    virtual void InitGame() override;
    virtual void InitRender() override;

    virtual void Teardown() override;
    virtual void Logic(GameCounter::TickUnit delta) override;
    virtual void OnInputEvent(const SystemEvent &event) override;

    virtual void OnFrameEnd(Frame *frame) override;

private:
    void HandleCompletedAssetBatch(Name, const RC<AssetBatch> &);
    void HandleCameraMovement(GameCounter::TickUnit delta);

    UniquePtr<RTCInstance>              m_rtc_instance;
    RC<RTCStream>                       m_rtc_stream;
    net::MessageQueue                   m_message_queue;

    Handle<Texture>                     m_texture;
    ByteBuffer                          m_screen_buffer;

    FlatMap<Name, RC<AssetBatch>>       m_asset_batches;

    uint                                m_counter = 0;
};

#endif