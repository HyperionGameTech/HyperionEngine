/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifdef HYP_COMMANDLET_NAME // only defined if this is being compiled as part of a commandlet target

#include <Core/Defines.hpp>

#ifndef HYP_STRINGIFY
#define HYP_STRINGIFY(x) HYP_STR(x)
#endif // HYP_STRINGIFY

extern "C"
{
    ENGINE_API int Hyp_Initialize(int argc, char** argv);
}

int main(int argc, char** argv)
{
    const int newArgc = argc + 2;

    char** newArgv = new char*[size_t(newArgc)];

    constexpr const char CommandletName[] = HYP_STRINGIFY(HYP_COMMANDLET_NAME);

    newArgv[0] = argv[0];

    newArgv[1] = const_cast<char*>("--exec");
    newArgv[2] = const_cast<char*>(CommandletName);

    for (int i = 1; i < argc; ++i)
    {
        newArgv[i + 2] = argv[i];
    }

    if (!Hyp_Initialize(newArgc, newArgv))
    {
        delete[] newArgv;
        return 1;
    }

    delete[] newArgv;

    return 0;
}

#endif
