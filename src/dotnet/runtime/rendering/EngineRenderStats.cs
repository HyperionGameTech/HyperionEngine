using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EngineRenderStats")]
    [StructLayout(LayoutKind.Sequential)]
    public struct EngineRenderStats
    {
        public double framesPerSecond;
        public double millisecondsPerFrame;
        public double millisecondsPerFrameAvg;
        public double millisecondsPerFrameMax;
        public double millisecondsPerFrameMin;
    }
}