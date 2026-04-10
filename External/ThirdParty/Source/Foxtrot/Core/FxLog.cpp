#include "FxLog.hpp"

#include <fstream>

static std::ofstream sCurrentLogFile;

std::ofstream& FxLogGetFile(bool* can_write)
{
    if (!sCurrentLogFile.is_open()) {
        FxLogToStdout<FxLogChannel::Error>("Attempting to write to log file that has not been opened");
        (*can_write) = false;
    }
    else {
        (*can_write) = true;
    }

    return sCurrentLogFile;
}

void FxLogCreateFile(const std::string& path) { sCurrentLogFile.open(path.c_str()); }
