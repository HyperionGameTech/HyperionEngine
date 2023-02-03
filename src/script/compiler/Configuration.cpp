#include <script/compiler/Configuration.hpp>

namespace hyperion::compiler {

const SizeType Config::max_data_members = 255;
const char *Config::global_module_name = "global";
bool Config::cull_unused_objects = false;

} // namespace hyperion::compiler
