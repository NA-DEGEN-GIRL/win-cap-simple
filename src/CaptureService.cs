using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Threading;

namespace WinCapSimple
{
    internal static class CaptureService
    {
        private static readonly ImageCodecInfo PngCodec = FindPngCodec();
        private static readonly EncoderParameters PngEncoderParameters = CreatePngEncoderParameters();

        public static Bitmap CaptureTarget(CaptureTarget target)
        {
            if (target == null)
            {
                throw new ArgumentNullException("target");
            }

            using (new FastCaptureScope())
            {
                if (target.Mode == CaptureMode.Freeform)
                {
                    return CaptureFreeform(target.FreeformPoints);
                }

                if (target.Mode == CaptureMode.Window && target.WindowHandle != IntPtr.Zero)
                {
                    Rectangle latest;
                    if (NativeMethods.TryGetWindowRectangle(target.WindowHandle, out latest))
                    {
                        target.Bounds = latest;
                    }
                }

                return CaptureBounds(target.Bounds);
            }
        }

        public static Bitmap CaptureBounds(Rectangle bounds)
        {
            if (bounds.Width <= 0 || bounds.Height <= 0)
            {
                throw new InvalidOperationException("Capture area is empty.");
            }

            Bitmap bitmap = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
            using (Graphics graphics = Graphics.FromImage(bitmap))
            {
                IntPtr hdc = graphics.GetHdc();
                try
                {
                    NativeMethods.CopyScreenToHdc(hdc, bounds.Left, bounds.Top, bounds.Width, bounds.Height);
                }
                finally
                {
                    graphics.ReleaseHdc(hdc);
                }
            }
            return bitmap;
        }

        public static Bitmap CaptureFreeform(IList<Point> screenPoints)
        {
            if (screenPoints == null || screenPoints.Count < 3)
            {
                throw new InvalidOperationException("Freeform selection needs at least three points.");
            }

            Rectangle bounds = BoundsFromPoints(screenPoints);
            Bitmap raw = CaptureBounds(bounds);
            Bitmap clipped = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);

            using (GraphicsPath path = new GraphicsPath())
            using (Graphics graphics = Graphics.FromImage(clipped))
            {
                Point[] relative = new Point[screenPoints.Count];
                for (int i = 0; i < screenPoints.Count; i++)
                {
                    relative[i] = new Point(screenPoints[i].X - bounds.Left, screenPoints[i].Y - bounds.Top);
                }

                path.AddPolygon(relative);
                graphics.Clear(Color.Transparent);
                graphics.SetClip(path);
                graphics.DrawImageUnscaled(raw, 0, 0);
            }

            raw.Dispose();
            return clipped;
        }

        public static Rectangle BoundsFromPoints(IList<Point> points)
        {
            int minX = points[0].X;
            int minY = points[0].Y;
            int maxX = points[0].X;
            int maxY = points[0].Y;

            for (int i = 1; i < points.Count; i++)
            {
                if (points[i].X < minX) minX = points[i].X;
                if (points[i].Y < minY) minY = points[i].Y;
                if (points[i].X > maxX) maxX = points[i].X;
                if (points[i].Y > maxY) maxY = points[i].Y;
            }

            return Rectangle.FromLTRB(minX, minY, Math.Max(minX + 1, maxX), Math.Max(minY + 1, maxY));
        }

        public static string CreateBurstFolder()
        {
            string root = Path.Combine(Path.GetTempPath(), "WinCapSimple", "Bursts");
            string folder = Path.Combine(root, DateTime.Now.ToString("yyyyMMdd_HHmmss"));
            Directory.CreateDirectory(folder);
            return folder;
        }

        public static List<string> CaptureBurst(CaptureTarget target, BurstSettings settings, string folder)
        {
            if (settings == null)
            {
                throw new ArgumentNullException("settings");
            }

            if (settings.DurationSeconds <= 0)
            {
                throw new InvalidOperationException("Burst duration must be positive.");
            }

            if (settings.IntervalMilliseconds < 50)
            {
                throw new InvalidOperationException("Burst interval is too small.");
            }

            Directory.CreateDirectory(folder);

            int frameCount = Math.Max(1, (settings.DurationSeconds * 1000) / settings.IntervalMilliseconds);
            List<string> files = new List<string>();

            using (new FastCaptureScope())
            {
                if (target.Mode == CaptureMode.Freeform)
                {
                    CaptureFreeformBurst(target, settings, folder, frameCount, files);
                }
                else
                {
                    CaptureRectangularBurst(target, settings, folder, frameCount, files);
                }
            }

            return files;
        }

        private static void CaptureRectangularBurst(CaptureTarget target, BurstSettings settings, string folder, int frameCount, List<string> files)
        {
            using (ReusableCaptureBuffer buffer = new ReusableCaptureBuffer(target.Bounds))
            {
                long nextTick = Stopwatch.GetTimestamp();
                long intervalTicks = MillisecondsToTicks(settings.IntervalMilliseconds);

                for (int i = 0; i < frameCount; i++)
                {
                    RefreshWindowBounds(target, buffer);
                    buffer.Capture();
                    string file = Path.Combine(folder, "frame_" + (i + 1).ToString("0000") + ".png");
                    SavePng(buffer.Bitmap, file);
                    files.Add(file);
                    nextTick += intervalTicks;
                    SleepUntil(nextTick);
                }
            }
        }

        private static void CaptureFreeformBurst(CaptureTarget target, BurstSettings settings, string folder, int frameCount, List<string> files)
        {
            Rectangle bounds = BoundsFromPoints(target.FreeformPoints);
            using (ReusableCaptureBuffer buffer = new ReusableCaptureBuffer(bounds))
            using (GraphicsPath path = CreateRelativePath(target.FreeformPoints, bounds))
            using (Bitmap clipped = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb))
            using (Graphics graphics = Graphics.FromImage(clipped))
            {
                graphics.SmoothingMode = SmoothingMode.None;
                graphics.CompositingMode = CompositingMode.SourceCopy;

                long nextTick = Stopwatch.GetTimestamp();
                long intervalTicks = MillisecondsToTicks(settings.IntervalMilliseconds);

                for (int i = 0; i < frameCount; i++)
                {
                    buffer.Capture();
                    graphics.ResetClip();
                    graphics.Clear(Color.Transparent);
                    graphics.SetClip(path);
                    graphics.DrawImageUnscaled(buffer.Bitmap, 0, 0);

                    string file = Path.Combine(folder, "frame_" + (i + 1).ToString("0000") + ".png");
                    SavePng(clipped, file);
                    files.Add(file);
                    nextTick += intervalTicks;
                    SleepUntil(nextTick);
                }
            }
        }

        public static string FindLatestBurstFolder()
        {
            string root = Path.Combine(Path.GetTempPath(), "WinCapSimple", "Bursts");
            if (!Directory.Exists(root))
            {
                return null;
            }

            DirectoryInfo rootInfo = new DirectoryInfo(root);
            DirectoryInfo[] folders = rootInfo.GetDirectories();
            Array.Sort(folders, delegate(DirectoryInfo left, DirectoryInfo right)
            {
                return right.CreationTimeUtc.CompareTo(left.CreationTimeUtc);
            });

            for (int i = 0; i < folders.Length; i++)
            {
                if (folders[i].GetFiles("*.png").Length > 0)
                {
                    return folders[i].FullName;
                }
            }

            return null;
        }

        private static void RefreshWindowBounds(CaptureTarget target, ReusableCaptureBuffer buffer)
        {
            if (target.Mode != CaptureMode.Window || target.WindowHandle == IntPtr.Zero)
            {
                return;
            }

            Rectangle latest;
            if (NativeMethods.TryGetWindowRectangle(target.WindowHandle, out latest))
            {
                target.Bounds = latest;
                buffer.SetSourceLocation(latest.Left, latest.Top);
            }
        }

        private static GraphicsPath CreateRelativePath(IList<Point> points, Rectangle bounds)
        {
            Point[] relative = new Point[points.Count];
            for (int i = 0; i < points.Count; i++)
            {
                relative[i] = new Point(points[i].X - bounds.Left, points[i].Y - bounds.Top);
            }

            GraphicsPath path = new GraphicsPath();
            path.AddPolygon(relative);
            return path;
        }

        private static void SavePng(Bitmap bitmap, string file)
        {
            if (PngCodec == null)
            {
                bitmap.Save(file, ImageFormat.Png);
            }
            else
            {
                bitmap.Save(file, PngCodec, PngEncoderParameters);
            }
        }

        private static ImageCodecInfo FindPngCodec()
        {
            ImageCodecInfo[] codecs = ImageCodecInfo.GetImageEncoders();
            for (int i = 0; i < codecs.Length; i++)
            {
                if (codecs[i].FormatID == ImageFormat.Png.Guid)
                {
                    return codecs[i];
                }
            }

            return null;
        }

        private static EncoderParameters CreatePngEncoderParameters()
        {
            EncoderParameters parameters = new EncoderParameters(1);
            parameters.Param[0] = new EncoderParameter(Encoder.Compression, 1L);
            return parameters;
        }

        private static long MillisecondsToTicks(int milliseconds)
        {
            return (long)((milliseconds / 1000.0) * Stopwatch.Frequency);
        }

        private static void SleepUntil(long targetTick)
        {
            long remainingTicks = targetTick - Stopwatch.GetTimestamp();
            if (remainingTicks <= 0)
            {
                return;
            }

            int remainingMs = (int)((remainingTicks * 1000L) / Stopwatch.Frequency);
            if (remainingMs > 1)
            {
                Thread.Sleep(remainingMs - 1);
            }

            while (Stopwatch.GetTimestamp() < targetTick)
            {
                Thread.SpinWait(20);
            }
        }

        private sealed class ReusableCaptureBuffer : IDisposable
        {
            private readonly Graphics _graphics;
            private readonly IntPtr _hdc;
            private readonly IntPtr _sourceHdc;
            private int _sourceLeft;
            private int _sourceTop;

            public readonly Rectangle Bounds;
            public readonly Bitmap Bitmap;

            public ReusableCaptureBuffer(Rectangle bounds)
            {
                if (bounds.Width <= 0 || bounds.Height <= 0)
                {
                    throw new InvalidOperationException("Capture area is empty.");
                }

                Bounds = bounds;
                _sourceLeft = bounds.Left;
                _sourceTop = bounds.Top;
                Bitmap = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
                _graphics = Graphics.FromImage(Bitmap);
                _hdc = _graphics.GetHdc();
                _sourceHdc = NativeMethods.AcquireDesktopHdc();
            }

            public void SetSourceLocation(int left, int top)
            {
                _sourceLeft = left;
                _sourceTop = top;
            }

            public void Capture()
            {
                NativeMethods.CopyDesktopHdcToHdc(_sourceHdc, _hdc, _sourceLeft, _sourceTop, Bounds.Width, Bounds.Height);
            }

            public void Dispose()
            {
                NativeMethods.ReleaseDesktopHdc(_sourceHdc);
                _graphics.ReleaseHdc(_hdc);
                _graphics.Dispose();
                Bitmap.Dispose();
            }
        }

        private sealed class FastCaptureScope : IDisposable
        {
            private readonly ThreadPriority _oldPriority;

            public FastCaptureScope()
            {
                Thread thread = Thread.CurrentThread;
                _oldPriority = thread.Priority;

                try
                {
                    thread.Priority = ThreadPriority.AboveNormal;
                }
                catch
                {
                }
            }

            public void Dispose()
            {
                try
                {
                    Thread.CurrentThread.Priority = _oldPriority;
                }
                catch
                {
                }
            }
        }
    }
}
