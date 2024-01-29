#include <util/Logger.hpp>
#include <util/StringUtil.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMString.hpp>

namespace hyperion {
namespace script {
namespace bindings {

static Logger g_script_logger(HYP_NAME_UNSAFE(script_logger_context));

static String FormatString(const String &str, const Array<vm::Value> &args)
{
    String result;

    int unnumbered_arg_index = 0;

    for (SizeType i = 0; i < str.Size(); ++i) {
        if (str[i] == '{') {
            if (i + 1 < str.Size() && str[i + 1] == '{') {
                result += '{';
                ++i;
            } else {
                SizeType j = i + 1;

                while (j < str.Size() && str[j] != '}') {
                    ++j;
                }

                if (j >= str.Size()) {
                    break;
                }

                String index_str = str.Substr(i + 1, j);

                int index = 0;

                if (index_str.Any()) {
                    if (!StringUtil::Parse<int>(index_str, &index)) {
                        i = j;

                        continue;
                    }
                } else {
                    index = unnumbered_arg_index++;
                }

                if (index < args.Size()) {
                    result += args[index].ToString().GetString();
                }

                i = j;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

static struct LoggerScriptBindings : ScriptBindingsBase
{
    LoggerScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Logger>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        static const LogChannel log_channel_debug(HYP_NAME(DEBUG));
        static const LogChannel log_channel_info(HYP_NAME(INFO));
        static const LogChannel log_channel_warn(HYP_NAME(WARN));
        static const LogChannel log_channel_error(HYP_NAME(ERROR));

        context.Class<Logger>("Logger")
            .StaticMember("DEBUG", "uint", vm::Value { vm::Value::U32, { .u32 = log_channel_debug.id } })
            .StaticMember("INFO", "uint", vm::Value { vm::Value::U32, { .u32 = log_channel_info.id } })
            .StaticMember("WARN", "uint", vm::Value { vm::Value::U32, { .u32 = log_channel_warn.id } })
            .StaticMember("ERROR", "uint", vm::Value { vm::Value::U32, { .u32 = log_channel_error.id } })
            .StaticMethod(
                "log",
                "function< int, Class, uint, String, varargs<any> >",
                [](sdk::Params params)
                {
                    HYP_SCRIPT_CHECK_ARGS(>=, 3);

                    vm::Number channel_id_num;
                    if (!params.args[1]->GetSignedOrUnsigned(&channel_id_num)) {
                        HYP_SCRIPT_THROW(vm::Exception ("log() expects a number as the first argument"));
                    }

                    const UInt64 channel_id = (channel_id_num.flags & vm::Number::FLAG_SIGNED)
                        ? UInt64(channel_id_num.i)
                        : channel_id_num.u;

                    vm::VMString *format_string_ptr = nullptr;
                    if (!params.args[2]->GetPointer<vm::VMString>(&format_string_ptr)) {
                        HYP_SCRIPT_THROW(vm::Exception ("log() expects a string as the first argument"));
                    }

                    Array<vm::Value> args_array;
                    args_array.Reserve(params.nargs - 3);

                    for (SizeType i = 3; i < params.nargs; ++i) {
                        args_array.PushBack(*params.args[i]);
                    }

                    String formatted_string = FormatString(
                        format_string_ptr->GetString(),
                        args_array
                    );

                    g_script_logger.Log(
                        LogChannel { channel_id },
                        formatted_string.Data()
                    );

                    HYP_SCRIPT_RETURN_INT32(params.nargs - 3);
                }
            )
            .StaticMethod(
                "set_channel_enabled",
                "function< void, Class, uint, bool >",
                CxxFn<
                    void, const void *, UInt32, bool,
                    [](const void *self, UInt32 channel_id, bool enabled) -> void
                    {
                        if (enabled) {
                            g_script_logger.SetLogMask(g_script_logger.GetLogMask() | (1ull << UInt64(channel_id)));
                        } else {
                            g_script_logger.SetLogMask(g_script_logger.GetLogMask() & ~(1ull << UInt64(channel_id)));
                        }
                    }
                >
            )
            .StaticMethod(
                "print",
                "function< void, Class, String >",
                CxxFn<
                    void, const void *, const String &,
                    [](const void *self, const String &str) -> void
                    {
                        std::printf("%s", str.Data());
                    }
                >
            )
            .Build();
    }

} logger_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion