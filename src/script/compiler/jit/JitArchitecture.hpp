//
// Created by emd22 on 24/08/22.
//

#ifndef HYPERION_JITARCHITECTURE_HPP
#define HYPERION_JITARCHITECTURE_HPP

#include <cstdint>

namespace hyperion::compiler::jit {

class JitArchitecture {
public:
    JitArchitecture() = default;
    ~JitArchitecture() = default;

    virtual void UploadArguments()
    {
        
    }
};

} // namespace hyperion::compiler::jit


#endif //HYPERION_JITARCHITECTURE_HPP
