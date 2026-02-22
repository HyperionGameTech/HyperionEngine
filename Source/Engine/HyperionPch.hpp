#pragma once

#ifdef __cplusplus

#include <Core/Types.hpp>
#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Constants.hpp>
#include <Core/Name.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/HashSet.hpp>
#include <Core/containers/String.hpp>
#include <Core/containers/FlatMap.hpp>
#include <Core/containers/FlatSet.hpp>
#include <Core/containers/Bitset.hpp>
#include <Core/containers/LinkedList.hpp>
#include <Core/containers/Queue.hpp>
#include <Core/containers/Stack.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/utilities/EnumFlags.hpp>

#endif // __cplusplus