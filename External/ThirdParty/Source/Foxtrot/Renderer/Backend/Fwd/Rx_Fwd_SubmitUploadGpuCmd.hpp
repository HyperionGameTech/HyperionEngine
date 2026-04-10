#pragma once

#include <functional>
#include "../RxCommands.hpp"


void Fx_Fwd_SubmitUploadCmd(std::function<void (RxCommandBuffer &)> func);
