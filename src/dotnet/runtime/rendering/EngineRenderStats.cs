using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public struct EngineRenderStatsCounts
    {
        public uint drawCalls;
        public uint triangles;
    }

    [HypClassBinding(Name="EngineRenderStats")]
    [StructLayout(LayoutKind.Sequential)]
    public struct EngineRenderStats
    {
        public double framesPerSecond;
        public double millisecondsPerFrame;
        public double millisecondsPerFrameAvg;
        public double millisecondsPerFrameMax;
        public double millisecondsPerFrameMin;
        public EngineRenderStatsCounts counts;
    }
}