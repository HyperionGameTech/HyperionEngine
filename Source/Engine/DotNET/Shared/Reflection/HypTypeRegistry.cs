using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public interface IHypTypeRegistry
    {
        static abstract void Register(HypTypeRegistryBuilder builder);
    }

    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = false)]
    public abstract class HypTypeRegistryAttribute : Attribute
    {
        public const int CurrentVersion = 1;

        public int Version { get; }

        protected HypTypeRegistryAttribute(int version)
        {
            Version = version;
        }

        public abstract void Register(HypTypeRegistryBuilder builder);
    }

    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
    public sealed class HypAssemblyDependencyAttribute : Attribute
    {
        public Type RegistryType { get; }

        public HypAssemblyDependencyAttribute(Type registryType)
        {
            RegistryType = registryType;
        }
    }

    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = false)]
    public sealed class HypTypeRegistryAttribute<T> : HypTypeRegistryAttribute where T : IHypTypeRegistry
    {
        public HypTypeRegistryAttribute(int version) : base(version)
        {
        }

        public override void Register(HypTypeRegistryBuilder builder)
        {
            T.Register(builder);
        }
    }

    public readonly struct HypMethodThunk
    {
        public readonly string Name;
        public readonly IntPtr Invoke;
        public readonly Attribute[]? Attributes;

        public unsafe HypMethodThunk(
            string name,
            delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void> invoke,
            Attribute[]? attributes)
        {
            Name = name;
            Invoke = (IntPtr)invoke;
            Attributes = attributes;
        }
    }

    public readonly struct HypPropertyThunk
    {
        public readonly string Name;
        public readonly IntPtr Getter;
        public readonly IntPtr Setter;
        public readonly Attribute[]? Attributes;

        // Set for [HypField] fields and [HypProperty] properties
        public readonly Type? MemberType;

        // A value of MemberType (default, empty string or empty array) boxed to find its native type; generated so NativeAOT needs no runtime construction
        public readonly object? SampleValue;

        public unsafe HypPropertyThunk(
            string name,
            delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void> getter,
            delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void> setter,
            Attribute[]? attributes)
            : this(name, getter, setter, attributes, null, null)
        {
        }

        public unsafe HypPropertyThunk(
            string name,
            delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void> getter,
            delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void> setter,
            Attribute[]? attributes,
            Type? memberType,
            object? sampleValue)
        {
            Name = name;
            Getter = (IntPtr)getter;
            Setter = (IntPtr)setter;
            Attributes = attributes;
            MemberType = memberType;
            SampleValue = sampleValue;
        }

        public HypMemberAttribute? MemberAttribute
        {
            get
            {
                if (MemberType == null || Attributes == null)
                {
                    return null;
                }

                foreach (Attribute attribute in Attributes)
                {
                    if (attribute is HypMemberAttribute memberAttribute)
                    {
                        return memberAttribute;
                    }
                }

                return null;
            }
        }
    }

    public sealed class HypTypeEntry
    {
        public Type Type { get; }
        public HypMethodThunk[] Methods { get; }
        public HypPropertyThunk[] Properties { get; }
        public IntPtr NewObject { get; }
        public IntPtr MarshalObject { get; }
        public IntPtr NativeSize { get; }
        public IntPtr MarshalOut { get; }

        internal HypTypeEntry(
            Type type,
            HypMethodThunk[] methods,
            HypPropertyThunk[] properties,
            IntPtr newObject,
            IntPtr marshalObject,
            IntPtr nativeSize,
            IntPtr marshalOut)
        {
            Type = type;
            Methods = methods;
            Properties = properties;
            NewObject = newObject;
            MarshalObject = marshalObject;
            NativeSize = nativeSize;
            MarshalOut = marshalOut;
        }
    }

    public sealed class HypFieldLayout
    {
        public string Name { get; }
        public Type FieldType { get; }
        public int Offset { get; }
        public int Size { get; }
        public bool IsPublic { get; }
        public bool IsNotSerialized { get; }
        public object DefaultValue { get; }

        public HypFieldLayout(
            string name,
            Type fieldType,
            int offset,
            int size,
            bool isPublic,
            bool isNotSerialized,
            object defaultValue)
        {
            Name = name;
            FieldType = fieldType;
            Offset = offset;
            Size = size;
            IsPublic = isPublic;
            IsNotSerialized = isNotSerialized;
            DefaultValue = defaultValue;
        }
    }

    public sealed unsafe class HypStructLayout
    {
        private readonly delegate*<IntPtr, object> _reader;
        private readonly delegate*<object, IntPtr, void> _writer;
        private readonly delegate*<object> _createDefault;
        private readonly delegate*<HypFieldLayout[]> _getFields;
        private HypFieldLayout[]? _fields;

        public int Size { get; }

        internal HypStructLayout(
            int size,
            delegate*<IntPtr, object> reader,
            delegate*<object, IntPtr, void> writer,
            delegate*<object> createDefault,
            delegate*<HypFieldLayout[]> getFields)
        {
            Size = size;
            _reader = reader;
            _writer = writer;
            _createDefault = createDefault;
            _getFields = getFields;
        }

        public HypFieldLayout[] Fields => _fields ??= _getFields();

        public object Read(IntPtr source) => _reader(source);

        public void Write(object value, IntPtr destination) => _writer(value, destination);

        public object CreateDefault() => _createDefault();
    }

    public sealed class HypTypeRegistryBuilder
    {
        internal readonly Dictionary<Type, HypTypeEntry> Entries = new();
        internal readonly Dictionary<Type, HypStructLayout> StructLayouts = new();

        public unsafe void AddStructLayout(
            Type type,
            int size,
            delegate*<IntPtr, object> reader,
            delegate*<object, IntPtr, void> writer,
            delegate*<object> createDefault,
            delegate*<HypFieldLayout[]> getFields)
        {
            StructLayouts[type] = new HypStructLayout(size, reader, writer, createDefault, getFields);
        }

        public unsafe void Add(
            Type type,
            HypMethodThunk[] methods,
            HypPropertyThunk[] properties,
            delegate*<IntPtr, IntPtr, object> newObject,
            delegate*<IntPtr, object> marshalObject,
            delegate*<int> nativeSize,
            delegate*<object, IntPtr, void> marshalOut)
        {
            Entries[type] = new HypTypeEntry(
                type,
                methods,
                properties,
                (IntPtr)newObject,
                (IntPtr)marshalObject,
                (IntPtr)nativeSize,
                (IntPtr)marshalOut);
        }
    }

    public static class HypTypeRegistry
    {
        private static readonly ConditionalWeakTable<Assembly, HypTypeRegistryBuilder> s_registriesByAssembly = new();
        private static readonly HypTypeRegistryBuilder s_emptyRegistry = new();

        public static HypTypeEntry? Get(Type type)
        {
            return GetRegistry(type.Assembly).Entries.TryGetValue(type, out HypTypeEntry? entry) ? entry : null;
        }

        public static HypStructLayout? GetStructLayout(Type type)
        {
            return GetRegistry(type.Assembly).StructLayouts.TryGetValue(type, out HypStructLayout? layout) ? layout : null;
        }

        public static bool HasRegistry(Assembly assembly)
        {
            return GetRegistry(assembly) != s_emptyRegistry;
        }

        public static IEnumerable<Type> GetTypes(Assembly assembly)
        {
            return GetRegistry(assembly).Entries.Keys;
        }

        private static HypTypeRegistryBuilder GetRegistry(Assembly assembly)
        {
            return s_registriesByAssembly.GetValue(assembly, LoadRegistry);
        }

        private static HypTypeRegistryBuilder LoadRegistry(Assembly assembly)
        {
            HypTypeRegistryAttribute? attribute = assembly.GetCustomAttribute<HypTypeRegistryAttribute>();

            if (attribute == null)
            {
                return s_emptyRegistry;
            }

            if (attribute.Version != HypTypeRegistryAttribute.CurrentVersion)
            {
                Logger.Log(LogLevel.Error, "Assembly {0} was built with type registry version {1}, but the engine expects version {2}. Rebuild it against this engine.",
                    assembly.GetName().Name, attribute.Version, HypTypeRegistryAttribute.CurrentVersion);

                return s_emptyRegistry;
            }

            HypTypeRegistryBuilder builder = new HypTypeRegistryBuilder();
            attribute.Register(builder);

            return builder;
        }
    }

    public static unsafe class HypMarshal
    {
        public static int SizeOf(Type type)
        {
            HypTypeEntry? entry = HypTypeRegistry.Get(type);

            if (entry != null && entry.NativeSize != IntPtr.Zero)
            {
                return ((delegate*<int>)entry.NativeSize)();
            }

            throw new NotSupportedException("No generated native size for " + type.FullName);
        }

        public static void StructureToPtr(object value, IntPtr destination)
        {
            HypTypeEntry? entry = HypTypeRegistry.Get(value.GetType());

            if (entry != null && entry.MarshalOut != IntPtr.Zero)
            {
                ((delegate*<object, IntPtr, void>)entry.MarshalOut)(value, destination);

                return;
            }

            throw new NotSupportedException("No generated marshaller for " + value.GetType().FullName);
        }
    }

    public static unsafe class HypThunkHelpers
    {
        public static void CheckArgCount(BoxedValueInternal** args, int count)
        {
            for (int i = 0; i < count; i++)
            {
                if (args == null || args[i] == null)
                {
                    throw new TargetParameterCountException("Expected " + count + " argument(s), got " + i);
                }
            }
        }

        public static T Arg<T>(BoxedValueInternal** args, int index)
        {
            return Convert<T>(args[index]->GetValue());
        }

        // a params array takes every argument from index up to the null that ends the list
        public static T[] ParamsArg<T>(BoxedValueInternal** args, int index)
        {
            if (args == null)
            {
                return Array.Empty<T>();
            }

            int count = 0;

            while (args[index + count] != null)
            {
                count++;
            }

            T[] values = new T[count];

            for (int i = 0; i < count; i++)
            {
                values[i] = Convert<T>(args[index + i]->GetValue());
            }

            return values;
        }

        public static void Return<T>(BoxedValueInternal* ret, T value)
        {
            if (ret != null)
            {
                ret->SetValue(value);
            }
        }

        public static T Convert<T>(object? value)
        {
            if (value is T typed)
            {
                return typed;
            }

            if (value == null)
            {
                return default!;
            }

            Type targetType = Nullable.GetUnderlyingType(typeof(T)) ?? typeof(T);

            if (targetType.IsEnum)
            {
                return (T)Enum.ToObject(targetType, value);
            }

            if (value is IConvertible && targetType.IsPrimitive)
            {
                return (T)System.Convert.ChangeType(value, targetType);
            }

            throw new InvalidCastException("Cannot convert argument of type " + value.GetType().Name + " to " + typeof(T).Name);
        }
    }
}
