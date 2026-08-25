using System;
using System.Diagnostics;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorViewModelFactory
    {
        private const string Vec2fName = "Vec2f";
        private const string Vec2iName = "Vec2i";
        private const string Vec2uName = "Vec2u";
        private const string Vec3fName = "Vec3f";
        private const string Vec3iName = "Vec3i";
        private const string Vec3uName = "Vec3u";
        private const string Vec4fName = "Vec4f";
        private const string Vec4iName = "Vec4i";
        private const string Vec4uName = "Vec4u";
        private const string TransformName = "Transform";
        private const string BoundingBoxName = "BoundingBox";
        private const string UuidName = "UUID";
        private const string MaterialTexturesName = "MaterialTextures";
        private const string BoolName = "bool";

        public static InspectorPropertyViewModelBase Create(
            ObjectBase? target,
            Property property,
            bool isReadOnly,
            int depth = 0,
            Action? preWriteCallback = null,
            Action? postWriteCallback = null,
            Action? valueChangedCallback = null)
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
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2fName)
            {
                vm = new Vec2fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2iName)
            {
                vm = new Vec2iViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2uName)
            {
                vm = new Vec2uViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3fName)
            {
                vm = new Vec3fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3iName)
            {
                vm = new Vec3iViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3uName)
            {
                vm = new Vec3uViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4fName)
            {
                vm = new Vec4fViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4iName)
            {
                vm = new Vec4iViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4uName)
            {
                vm = new Vec4uViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == TransformName)
            {
                vm = new TransformViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == BoundingBoxName)
            {
                vm = new BoundingBoxPropertyViewModel(target, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == UuidName)
            {
                vm = new ReadOnlyPropertyViewModel(target, property, isReadOnly: true);
            }
            else if (typeInfo.Class?.Name == MaterialTexturesName)
            {
                vm = new MaterialTexturesPropertyViewModel(target, property, isReadOnly, depth);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == BoolName)
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

            AttachCallbacks(vm, preWriteCallback, postWriteCallback, valueChangedCallback);

            return Initialize(vm);
        }

        // initialize: pass false when the caller will call RefreshValue() manually later
        // (e.g. StructPropertyViewModel defers refresh until its struct copy is loaded).
        public static InspectorPropertyViewModelBase CreateForComponent(
            IntPtr classAddress,
            Func<IntPtr> targetAddressResolver,
            Property property,
            bool isReadOnly,
            int depth = 0,
            bool initialize = true,
            Action? preWriteCallback = null,
            Action? postWriteCallback = null,
            Action? valueChangedCallback = null)
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
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2fName)
            {
                vm = new Vec2fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2iName)
            {
                vm = new Vec2iViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2uName)
            {
                vm = new Vec2uViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3fName)
            {
                vm = new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3iName)
            {
                vm = new Vec3iViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3uName)
            {
                vm = new Vec3uViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4fName)
            {
                vm = new Vec4fViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4iName)
            {
                vm = new Vec4iViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4uName)
            {
                vm = new Vec4uViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == TransformName)
            {
                vm = new TransformViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == BoundingBoxName)
            {
                vm = new BoundingBoxPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly);
            }
            else if (typeInfo.Class?.Name == UuidName)
            {
                vm = new ReadOnlyPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly: true);
            }
            else if (typeInfo.Class?.Name == MaterialTexturesName)
            {
                vm = new MaterialTexturesPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, depth);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == BoolName)
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

            AttachCallbacks(vm, preWriteCallback, postWriteCallback, valueChangedCallback);

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
            Action? preWriteCallback = null,
            Action? postWriteCallback = null,
            Action? valueChangedCallback = null)
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
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2fName)
            {
                vm = new Vec2fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2iName)
            {
                vm = new Vec2iViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec2 && typeInfo.Class?.Name == Vec2uName)
            {
                vm = new Vec2uViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3fName)
            {
                vm = new Vec3fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3iName)
            {
                vm = new Vec3iViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec3 && typeInfo.Class?.Name == Vec3uName)
            {
                vm = new Vec3uViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4fName)
            {
                vm = new Vec4fViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4iName)
            {
                vm = new Vec4iViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.IsVec4 && typeInfo.Class?.Name == Vec4uName)
            {
                vm = new Vec4uViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.Class?.Name == BoundingBoxName)
            {
                vm = new BoundingBoxPropertyViewModel(label, typeInfo, getter, setter, isReadOnly);
            }
            else if (typeInfo.Class?.Name == UuidName)
            {
                vm = new ReadOnlyPropertyViewModel(label, getter, setter, isReadOnly: true);
            }
            else if (typeInfo.Class?.Name == MaterialTexturesName)
            {
                vm = new MaterialTexturesPropertyViewModel(label, typeInfo, getter, setter, isReadOnly, depth);
            }
            else if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == BoolName)
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

            AttachCallbacks(vm, preWriteCallback, postWriteCallback, valueChangedCallback);

            return initialize ? Initialize(vm) : vm;
        }

        private static void AttachCallbacks(
            InspectorPropertyViewModelBase viewModel,
            Action? preWriteCallback,
            Action? postWriteCallback,
            Action? valueChangedCallback)
        {
            viewModel.PreWriteCallback = preWriteCallback;
            viewModel.PostWriteCallback = postWriteCallback;
            viewModel.ValueChangedCallback = valueChangedCallback;
        }

        private static InspectorPropertyViewModelBase Initialize(InspectorPropertyViewModelBase viewModel)
        {
            viewModel.RefreshValue();
            return viewModel;
        }
    }
}
