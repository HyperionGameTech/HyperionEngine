#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>

namespace hyperion::v2 {

void AssetBatch::LoadAsync(UInt num_batches)
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    if (enqueued_assets->Empty()) {
        return;
    }

    // partition each proc
    AssertThrow(procs.Size() == enqueued_assets->Size());

    const UInt num_items = UInt(procs.Size());

    num_batches = MathUtil::Max(num_batches, 1u);
    num_batches = MathUtil::Min(num_batches, num_items);

    const UInt items_per_batch = num_items / num_batches;

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        const UInt offset_index = batch_index * items_per_batch;
        const UInt max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

        AssertThrow(max_index >= offset_index);

        Array<Proc<void, AssetManager *, AssetMap *>> batch_procs;
        batch_procs.Reserve(max_index - offset_index);

        for (UInt i = offset_index; i < max_index; ++i) {
            batch_procs.PushBack(std::move(procs[i]));
        }

        AddTask([asset_manager = asset_manager, batch_procs = std::move(batch_procs), asset_map = enqueued_assets.Get()](...) mutable {
            for (auto &proc : batch_procs) {
                proc(asset_manager, asset_map);
            }
        });
    }

    procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);
}

AssetMap AssetBatch::AwaitResults()
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    AssetMap results = std::move(*enqueued_assets);
    // enqueued_assets is cleared now

    return results;
}

AssetMap AssetBatch::ForceLoad()
{
    AssertThrowMsg(enqueued_assets != nullptr, "AssetBatch is in invalid state");

    for (auto &proc : procs) {
        proc(asset_manager, enqueued_assets.Get());
    }

    procs.Clear();

    AssetMap results = std::move(*enqueued_assets);
    // enqueued_assets is cleared now

    return results;
}

static RC<AssetBatch> ScriptCreateAssetBatch(void *)
{
    return g_asset_manager->CreateBatch();
}


static struct EnqueuedAssetScriptBindings : ScriptBindingsBase
{
    EnqueuedAssetScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<EnqueuedAsset>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<EnqueuedAsset>(
                "EnqueuedAsset",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxCtor< EnqueuedAsset >
                    ),
                    API::NativeMemberDefine(
                        "AsNode",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< NodeProxy, EnqueuedAsset, &EnqueuedAsset::ExtractAs<Node> >
                    ),
                    API::NativeMemberDefine(
                        "AsTexture",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< Handle<Texture>, EnqueuedAsset, &EnqueuedAsset::ExtractAs<Texture> >
                    ),
                    API::NativeMemberDefine(
                        "AsScript",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFn< Handle<Script>, EnqueuedAsset, &EnqueuedAsset::ExtractAs<Script> >
                    )
                }
            );
    }
} enqueued_asset_script_bindings = { };

static struct AssetMapScriptBindings : ScriptBindingsBase
{
    AssetMapScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<AssetMap>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<AssetMap>(
                "AssetMap",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxCtor< AssetMap >
                    ),
                    API::NativeMemberDefine(
                        "Get",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "name", BuiltinTypes::STRING }
                        },
                        CxxMemberFn< EnqueuedAsset &, AssetMap, const String &, &AssetMap::operator[] >
                    )
                }
            );
    }
} asset_map_script_bindings = { };

static struct AssetBatchScriptBindings : ScriptBindingsBase
{
    AssetBatchScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<AssetBatch>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<RC<AssetBatch>>(
                "AssetBatch",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< RC<AssetBatch>, void *, ScriptCreateAssetBatch >
                    ),
                    API::NativeMemberDefine(
                        "AddNode",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "key", BuiltinTypes::STRING },
                            { "path", BuiltinTypes::STRING }
                        },
                        CxxMemberFnWrapped< void, RC<AssetBatch>, AssetBatch, const String &, const String &, &AssetBatch::Add<Node> >
                    ),
                    API::NativeMemberDefine(
                        "AddTexture",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "key", BuiltinTypes::STRING },
                            { "path", BuiltinTypes::STRING }
                        },
                        CxxMemberFnWrapped< void, RC<AssetBatch>, AssetBatch, const String &, const String &, &AssetBatch::Add<Texture> >
                    ),
                    API::NativeMemberDefine(
                        "LoadAsync",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "num_batches", BuiltinTypes::UNSIGNED_INT, RC<AstExpression>(new AstUnsignedInteger(~0u, SourceLocation::eof)) }
                        },
                        CxxMemberFnWrapped< void, RC<AssetBatch>, AssetBatch, UInt, &AssetBatch::LoadAsync >
                    ),
                    API::NativeMemberDefine(
                        "AwaitResults",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< AssetMap, RC<AssetBatch>, AssetBatch, &AssetBatch::AwaitResults >
                    )
                }
            );
    }
} asset_batch_script_bindings = { };

} // namespace hyperion::v2