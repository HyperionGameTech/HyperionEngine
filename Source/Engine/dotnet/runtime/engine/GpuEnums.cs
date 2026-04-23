namespace Hyperion
{
    [ClassBinding(Name = "GpuType")]
    public enum GpuType : byte
    {
        Unknown = 0,
        Integrated,
        Dedicated
    }

    [ClassBinding(Name = "GpuVendor")]
    public enum GpuVendor : byte
    {
        Unknown = 0,
        Nvidia,
        Amd,
        Intel,
        Qualcomm,
        Apple,
        Microsoft
    }
}