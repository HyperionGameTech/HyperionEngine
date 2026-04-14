using System;
using System.Diagnostics;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorViewModelFactory
    {
        public static InspectorPropertyViewModelBase Create(ObjectBase? target, Property property, bool isReadOnly, int depth = 0)
        {
            if (target == null)
            {
                throw new ArgumentNullException(nameof(target));
            }

            TypeInfo typeInfo = property.TypeInfo;
            bool isNameType = InspectorPropertyViewModelBase.IsNameType(typeInfo);

            if (typeInfo.IsString || isNameType)
            {
                return Initialize(new TextPropertyViewModel(target, property, isReadOnly, isNameType));
            }

            if (typeInfo.IsEnumFlags)
            {
                return Initialize(new FlagsPropertyViewModel(target, property, typeInfo.Class, isReadOnly));
            }

            if (typeInfo.IsEnum)
            {
                return Initialize(new EnumPropertyViewModel(target, property, typeInfo.Class, isReadOnly));
            }

            if (typeInfo.IsVec2 && typeInfo.Name == "Vec2f")
            {
                return Initialize(new Vec2fViewModel(target, property, isReadOnly));
            }

            if (typeInfo.IsVec3 && typeInfo.Name == "Vec3f")
            {
                return Initialize(new Vec3fViewModel(target, property, isReadOnly));
            }

            if (typeInfo.IsVec4 && typeInfo.Name == "Vec4f")
            {
                return Initialize(new Vec4fViewModel(target, property, isReadOnly));
            }

            if (typeInfo.Class?.Name == "Transform")
            {
                return Initialize(new TransformViewModel(target, property, isReadOnly));
            }

            if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == "bool")
            {
                return Initialize(new BoolPropertyViewModel(target, property, isReadOnly));
            }

            if (typeInfo.IsFundamental && (typeInfo.IsIntegral || typeInfo.IsFloat))
            {
                return Initialize(new NumericPropertyViewModel(target, property, isReadOnly));
            }

            if (typeInfo.IsClass && typeInfo.Class.HasValue)
            {
                return Initialize(new ObjectPropertyViewModel(target, property, isReadOnly, depth));
            }

            Logger.Log(LogLevel.Debug, $"Inspector creating read-only property view model for property '{property.Name}' of type '{typeInfo.Name}'");

            return Initialize(new ReadOnlyPropertyViewModel(target, property, isReadOnly));
        }

        public static InspectorPropertyViewModelBase CreateForComponent(
            IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
        {
            TypeInfo typeInfo = property.TypeInfo;
            bool isNameType = InspectorPropertyViewModelBase.IsNameType(typeInfo);

            if (typeInfo.IsString || isNameType)
            {
                return Initialize(new TextPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, isNameType));
            }

            if (typeInfo.IsEnumFlags)
            {
                return Initialize(new FlagsPropertyViewModel(classAddress, targetAddressResolver, property, typeInfo.Class, isReadOnly));
            }

            if (typeInfo.IsEnum)
            {
                return Initialize(new EnumPropertyViewModel(classAddress, targetAddressResolver, property, typeInfo.Class, isReadOnly));
            }

            if (typeInfo.IsVec2 && typeInfo.Name == "Vec2f")
            {
                return Initialize(new Vec2fViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.IsVec3 && typeInfo.Name == "Vec3f")
            {
                return Initialize(new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.IsVec4 && typeInfo.Name == "Vec4f")
            {
                return Initialize(new Vec4fViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.Class?.Name == "Transform")
            {
                return Initialize(new TransformViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.IsFundamental && typeInfo.IsIntegral && typeInfo.Name == "bool")
            {
                return Initialize(new BoolPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.IsFundamental && (typeInfo.IsIntegral || typeInfo.IsFloat))
            {
                return Initialize(new NumericPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly));
            }

            if (typeInfo.IsClass && typeInfo.Class.HasValue)
            {
                return Initialize(new ObjectPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly, depth));
            }

            Logger.Log(LogLevel.Debug, $"Inspector creating read-only component property view model for property '{property.Name}' of type '{typeInfo.Name}'");

            return Initialize(new ReadOnlyPropertyViewModel(classAddress, targetAddressResolver, property, isReadOnly));
        }

        private static InspectorPropertyViewModelBase Initialize(InspectorPropertyViewModelBase viewModel)
        {
            viewModel.RefreshValue();
            return viewModel;
        }
    }
}

            