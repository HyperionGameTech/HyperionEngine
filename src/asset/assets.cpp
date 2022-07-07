#include "Assets.hpp"
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

Assets::Assets(Engine *engine)
    : m_engine(engine),
      m_base_path(FileSystem::CurrentPath())
{
}

Assets::~Assets()
{
    
}

} // namespace hyperion::v2