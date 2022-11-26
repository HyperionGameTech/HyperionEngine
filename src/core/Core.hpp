#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/lib/CMemory.hpp>
#include <system/Debug.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::renderer {

class Device;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;
class ObjectPool;

template <class T>
struct Handle;

Engine *GetEngine();

renderer::Device *GetEngineDevice();

ObjectPool &GetObjectPool();

template <class T, class Engine>
static HYP_FORCE_INLINE bool InitObjectIntern(Engine *engine, Handle<T> &handle)
{
    return engine->template InitObject<T>(handle);
}

template <class T>
static HYP_FORCE_INLINE bool InitObject(Handle<T> &handle)
{
    return InitObjectIntern(GetEngine(), handle);
}

template <class T, class Engine>
static HYP_FORCE_INLINE bool CreateObjectIntern(Engine *engine)
{
    return engine->template CreateObject<T>();
}

template <class T>
static HYP_FORCE_INLINE bool CreateObject()
{
    return CreateObjectIntern<T>(GetEngine());
}

} // namespace hyperion::v2

#endif