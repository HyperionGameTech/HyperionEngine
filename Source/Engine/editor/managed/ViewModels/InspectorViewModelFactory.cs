using System;
using System.Diagnostics;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorViewModelFactory
    {
        public static InspectorPropertyViewModelBase Create(ObjectBase? target, Property property, bool isReadOnly, int depth = 0, Action? postWriteCallback = null)
        {
            if (target == null)
            {
                throw new ArgumentNullException(nameof(target));
            }

            TypeInfo typeInfo = property.TypeInfo;
            bool isNameType = InspectorPropertyViewModelBase.IsNameType(typeInfo);

            InspectorPropertyViewModelBase vm;

            if (typeInfo.IsArray)
            {
                vm = new ArrayPropertyViewModel(target, property, isReadOnly, depth);
            }
            else if (typeInfo.IsString || isNameType)
            {
                vm = new TextPropertyViewModel(target, property, isReadOnly, isNameType);
            }
            else if (typeInfo.IsEnumFlags)
            {
                vm = new FlagsPropertyViewModel(target, property, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsEnum)
            {
                vm = new EnumPropertyViewModel(target, property, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == "Vec2f")
            {
                vm = new Vec2fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == "Vec3f")
            {
                vm = new Vec3fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == "Vec4f")
            {
                vm = new Vec4fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == "Transform")
            {
                vm = new TransformViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == "bool")
            {
                vm = new BoolPropertyViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsFundamental && (typeInfo.IsIntegral || typeInfo.IsFloat))
            {
                vm = new NumericPropertyViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsClassType)
            {
                vm = new ObjectPropertyViewModel(target, property, isReadOnly, depth);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsStructType)
            {
                vm = new StructPropertyViewModel(target, property, isReadOnly, depth);
            }
            else
            {
                Logger.Log(LogLevel.Debug, $"Inspector creating read-only property view model for property '{property.Name}' of type '{typeInfo.Name}'");
                vm = new ReadOnlyPropertyViewModel(target, property, isReadOnly);
            }

            vm.PostWriteCallback = postWriteCallback;

            return Initialize(vm);
        }

        // initialize: pass false when the caller will call RefreshValue() manually later
        // (e.g. StructPropertyViewModel defers refresh until its struct copy is loaded).
        public static InspectorPropertyViewModelBase CreateForComponent(
            IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0, bool initialize = true, Action? postWriteCallback = null)
        {
            TypeInfo typeInfo = property.TypeInfo;
            bool isNameType = InspectorPropertyViewModelBase.IsNameType(typeInfo);

            InspectorPropertyViewModelBase vm;

            if (typeInfo.IsArray)
            {
                vm = new ArrayPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, depth);
            }
            else if (typeInfo.IsString || isNameType)
            {
                vm = new TextPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, isNameType);
            }
            else if (typeInfo.IsEnumFlags)
            {
                vm = new FlagsPropertyViewModel(classAddress, targetAddressResolver, property, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsEnum)
            {
                vm = new EnumPropertyViewModel(classAddress, targetAddressResolver, property, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == "Vec2f")
            {
                vm = new Vec2fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == "Vec3f")
            {
                vm = new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == "Vec4f")
            {
                vm = new Vec4fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == "Transform")
            {
                vm = new TransformViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == "bool")
            {
                vm = new BoolPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsFundamental && (typeInfo.IsIntegral || typeInfo.IsFloat))
            {
                vm = new NumericPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsClassType)
            {
                vm = new ObjectPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, depth);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsStructType)
            {
                vm = new StructPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, depth);
            }
            else
            {
                Logger.Log(LogLevel.Debug, $"Inspector creating read-only property view model for property '{property.Name}' of type '{typeInfo.Name}'");
                vm = new ReadOnlyPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }

            vm.PostWriteCallback = postWriteCallback;

            return initialize ? Initialize(vm) : vm;
        }

        public static InspectorPropertyViewModelBase CreateForValue(
            string label,
            TypeInfo typeInfo,
            Func<BoxedValue> getter,
            Action<BoxedValue> setter,
            bool isReadOnly = false,
            int depth = 0,
            bool initialize = true,
            Action? postWriteCallback = null)
        {
            bool isNameType = InspectorPropertyViewModelBase.IsNameType(typeInfo);

            InspectorPropertyViewModelBase vm;

            if (typeInfo.IsArray)
            {
                vm = new ArrayPropertyViewModel(label, typeInfo, getter, setter, isReadOnly, depth);
            }
            else if (typeInfo.IsString || isNameType)
            {
                vm = new TextPropertyViewModel(label, typeInfo, getter, setter, isReadOnly, isNameType);
            }
            else if (typeInfo.IsEnumFlags)
            {
                vm = new FlagsPropertyViewModel(label, typeInfo, getter, setter, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsEnum)
            {
                vm = new EnumPropertyViewModel(label, typeInfo, getter, setter, typeInfo.Class, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == "Vec2f")
            {
                vm = new Vec2fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == "Vec3f")
            {
                vm = new Vec3fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == "Vec4f")
            {
                vm = new Vec4fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == "bool")
            {
                vm = new BoolPropertyViewModel(label, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsFundamental && (typeInfo.IsIntegral || typeInfo.IsFloat))
            {
                vm = new NumericPropertyViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsClassType)
            {
                vm = new ObjectPropertyViewModel(label, typeInfo, getter, setter, isReadOnly, depth);
            }
            else if (typeInfo.Class.HasValue && typeInfo.Class.Value.IsStructType)
            {
                vm = new StructPropertyViewModel(label, typeInfo, getter, setter, isReadOnly, depth);
            }
            else
            {
                Logger.Log(LogLevel.Debug, $"Inspector creating read-only value view model for label '{label}' of type '{typeInfo.Name}'");
                vm = new ReadOnlyPropertyViewModel(label, getter, setter, isReadOnly);
            }

            vm.PostWriteCallback = postWriteCallback;

            return initialize ? Initialize(vm) : vm;
        }

        private static InspectorPropertyViewModelBase Initialize(InspectorPropertyViewModelBase viewModel)
        {
            viewModel.RefreshValue();
            return viewModel;
        }
    }
}
