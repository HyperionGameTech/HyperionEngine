#include <builders/shader_compiler/ShaderCompiler.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

template <class Container, class Lambda>
void Permute(Container &container, UInt start_index, UInt end_index, Lambda &&lambda)
{
    if (start_index == end_index) {
        lambda(container, start_index, end_index + 1);

        return;
    }

    for (UInt i = start_index; i < end_index; ++i) {
        std::swap(container[i], container[start_index]);
        Permute(container, start_index + 1, end_index, lambda);
        std::swap(container[i], container[start_index]);
    }
}

void ShaderCompiler::CompileBundles()
{
    for (auto &bundle : m_bundles) {
        AssertThrow(CompileBundle(bundle));
    }
}

bool ShaderCompiler::CompileBundle(Bundle &bundle)
{
    // run with spirv-cross
    // FileSystem::Mkdir("data/compiled_shaders/tmp");
    // for (auto &file : bundle.files) {
    //     if (system((String("glslc --target-env=vulkan1.1 ") + file.path + " -o data/compiled_shaders/tmp/" + FilePath(file.path).Basename() + ".spv").Data())
    // }



    HashCode hc;

    for (auto &file : bundle.files) {
        hc.Add(file);
    }

    hc.Add(bundle.props);

    String hash_code_str = String::ToString(hc.Value());

    // compile each file with different props enabled

    SizeType num_permutations = 1;

    for (SizeType i = bundle.props.Size(); i > 0; --i) {
        num_permutations *= i;
    }

    if (num_permutations >= 64) {
        // too many permutations
        return false;
    }

    // compile each..
    DynArray<String> props;
    props.Reserve(bundle.props.Size());

    for (auto &item : bundle.props) {
        props.PushBack(item);
    }

    std::cout << "====\n\n";
    Permute(props, 0, props.Size() - 1, [](auto &container, UInt start_index, UInt end_index) {
        for (UInt i = start_index; i < end_index; ++i) {
            auto &item = *(container.Data() + i);
            std::cout << "item : " << item.GetHashCode().Value() << "\n";
        }
    });

    return true;
}

} // namespace hyperion::v2