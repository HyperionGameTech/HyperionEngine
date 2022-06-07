#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include <core/containers.h>
#include <rendering/backend/renderer_instance.h>
#include <hash_code.h>
#include <util/defines.h>

#include <memory>
#include <atomic>


namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

class Engine;

Device *GetEngineDevice(Engine *engine);

template <class T>
struct Stub {
    Stub() = default;
    ~Stub() = default;

    renderer::Result Create(Engine *)  { return renderer::Result::OK; }
    renderer::Result Destroy(Engine *) { return renderer::Result::OK; }
};

#define STUB_CLASS(name) ::hyperion::v2::Stub<name>

#define ENGINE_COMPONENT_DELEGATE_METHODS \
    decltype(m_wrapped) *operator->()             { return &m_wrapped; } \
    const decltype(m_wrapped) *operator->() const { return &m_wrapped; }

// a non-descript ID (no type attached)
struct IDBase {
    using ValueType = uint32_t;
    
    explicit constexpr operator ValueType() const { return value; }
    constexpr ValueType Value() const             { return value; }
    
    explicit constexpr operator bool() const      { return bool(value); }

    constexpr bool operator==(const IDBase &other) const
        { return value == other.value; }

    constexpr bool operator!=(const IDBase &other) const
        { return value != other.value; }

    constexpr bool operator<(const IDBase &other) const
        { return value < other.value; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }

    ValueType value{0};
};

template <class Type>
struct ID : IDBase {};

template <class Type>
class EngineComponentBase : public CallbackTrackable<EngineCallbacks> {
public:
    using ID = ID<Type>;

    static constexpr ID empty_id = ID{0};

    EngineComponentBase()
        : CallbackTrackable(),
          m_id(empty_id),
          m_init_called(false),
          m_is_ready(false),
          m_engine(nullptr)
    {
    }

    EngineComponentBase(const EngineComponentBase &other) = delete;
    EngineComponentBase &operator=(const EngineComponentBase &other) = delete;
    ~EngineComponentBase() = default;

    ID GetId() const          { return m_id; }

    /* To be called from ObjectHolder<Type> */
    void SetId(const ID &id)  { m_id = id; }
    bool IsInitCalled() const { return m_init_called; }
    bool IsReady() const      { return m_is_ready; }

    /*! \brief Just a function to store that Init() has been called from a derived class
     * for book-keeping. Use to prevent adding OnInit() callbacks multiple times.
     */
    void Init(Engine *engine)
    {
        AssertThrowMsg(
            engine != nullptr,
            "Engine was nullptr!"
        );

        m_init_called = true;
        m_engine      = engine;
    }

protected:
    using Base = EngineComponentBase<Type>;

    void Destroy()
    {
        m_init_called = false;
        m_engine      = nullptr;
    }
    
    void SetReady(bool is_ready) { m_is_ready = is_ready; }

    HYP_FORCE_INLINE void AssertReady() const
    {
        AssertThrowMsg(
            m_is_ready,
            "Component is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n"
        );
    }
    
    HYP_FORCE_INLINE Engine *GetEngine() const
    {
        AssertThrowMsg(
            m_engine != nullptr,
            "GetEngine() called when engine is not set! This indicates using a component which has not had Init() called on it."
        );

        return m_engine;
    }

    ID m_id;
    std::atomic_bool m_init_called;
    std::atomic_bool m_is_ready;
    Engine *m_engine;
};

template <class WrappedType>
class EngineComponent : public EngineComponentBase<WrappedType> {
public:
    template <class ...Args>
    EngineComponent(Args &&... args)
        : EngineComponentBase<WrappedType>(),
          m_wrapped(std::move(args)...),
          m_wrapped_created(false)
    {
#if HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif
    }

    EngineComponent(const EngineComponent &other) = delete;
    EngineComponent &operator=(const EngineComponent &other) = delete;

    ~EngineComponent()
    {
        const char *type_name = typeid(WrappedType).name();

#if 0 // HYP_DEBUG_MODE
        AssertThrowMsg(
            m_wrapped_destroyed,
            "Expected wrapepd object of type %s to have been destroyed before destructor",
            type_name
        );
#endif

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr",
            type_name
        );
    }

    WrappedType &Get()             { return m_wrapped; }
    const WrappedType &Get() const { return m_wrapped; }

    /* Standard non-specialized initialization function */
    template <class ...Args>
    void Create(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to have not already been created, but it was already created.",
            wrapped_type_name
        );

        auto result = m_wrapped.Create(GetEngineDevice(engine), std::forward<Args>(args)...);
        AssertThrowMsg(
            result,
            "Creation of object of type %s failed.\n\tError Code: %d\n\tMessage: %s",
            wrapped_type_name,
            result.error_code,
            result.message
        );

        m_wrapped_created = true;

#if HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif

        EngineComponentBase<WrappedType>::Init(engine);
    }

    /* Standard non-specialized destruction function */
    template <class ...Args>
    void Destroy(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

#if HYP_DEBUG_MODE
        AssertThrowMsg(
            !m_wrapped_destroyed,
            "Wrapped object of type %s was already destroyed",
            wrapped_type_name
        );
#endif

        AssertThrowMsg(
            m_wrapped_created,
            "Expected wrapped object of type %s to have been created, but it was not yet created (or it was already destroyed)",
            wrapped_type_name
        );

        auto result = m_wrapped.Destroy(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Destruction of object of type %s failed: %s", wrapped_type_name, result.message);

        m_wrapped_created = false;
        
#if HYP_DEBUG_MODE
        m_wrapped_destroyed = true;
#endif

        EngineComponentBase<WrappedType>::Destroy();
    }

protected:
    WrappedType m_wrapped;
    bool m_wrapped_created;
    bool m_wrapped_destroyed; /* for debug purposes; not really used for anything outside (as objects should be able to be destroyed then recreated) */
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPONENTS_BASE_H

