using System;
using System.Collections.Generic;
using System.Drawing;

namespace WinCapSimple
{
    internal enum CaptureMode
    {
        Rectangle,
        Freeform,
        Window,
        Display
    }

    internal sealed class CaptureTarget
    {
        public CaptureMode Mode;
        public Rectangle Bounds;
        public IntPtr WindowHandle;
        public List<Point> FreeformPoints;
        public string Description;
    }

    internal sealed class BurstSettings
    {
        public int DurationSeconds;
        public int IntervalMilliseconds;
    }

    internal sealed class WindowInfo
    {
        public IntPtr Handle;
        public string Title;
        public Rectangle Bounds;

        public override string ToString()
        {
            return Title + "  (" + Bounds.Width + "x" + Bounds.Height + ")";
        }
    }

    internal sealed class ScreenItem
    {
        public ScreenItem(int index, Rectangle bounds, bool primary)
        {
            Index = index;
            Bounds = bounds;
            Primary = primary;
        }

        public int Index;
        public Rectangle Bounds;
        public bool Primary;

        public override string ToString()
        {
            string suffix = Primary ? " primary" : "";
            return "Display " + (Index + 1) + suffix + "  " + Bounds.Width + "x" + Bounds.Height;
        }
    }

    internal sealed class DelayItem
    {
        public DelayItem(string label, int milliseconds)
        {
            Label = label;
            Milliseconds = milliseconds;
        }

        public string Label;
        public int Milliseconds;

        public override string ToString()
        {
            return Label;
        }
    }

    internal sealed class InkStroke
    {
        public readonly List<PointF> Points = new List<PointF>();
        public Color Color;
        public float Width;
    }
}
