using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    /// <summary>
    /// Indicates that a struct does not have a native class representation. Used by the NativeInterop system to avoid trying to hook up a native class for this type.
    /// </summary>
    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class NoNativeClass : Attribute
    {
    }

    /// <summary>
    /// Indicates that a method or property should only be called from the main thread.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Property, Inherited = true)]
    public class MainThreadOnly : Attribute
    {
    }

    /// <summary>
    /// Indicates that a method or property should only be called from the game thread.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Property, Inherited = true)]
    public class GameThreadOnly : Attribute
    {
    }
}
