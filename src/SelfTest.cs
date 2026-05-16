using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal static class SelfTest
    {
        public static int Run()
        {
            string folder = null;

            try
            {
                Rectangle screen = Screen.PrimaryScreen.Bounds;
                Rectangle probe = new Rectangle(screen.Left, screen.Top, Math.Min(8, screen.Width), Math.Min(8, screen.Height));

                using (Bitmap bitmap = CaptureService.CaptureBounds(probe))
                {
                    if (bitmap.Width != probe.Width || bitmap.Height != probe.Height)
                    {
                        return 2;
                    }
                }

                folder = CaptureService.CreateBurstFolder();
                CaptureTarget target = new CaptureTarget();
                target.Mode = CaptureMode.Display;
                target.Bounds = probe;
                target.Description = "Self-test";

                BurstSettings settings = new BurstSettings();
                settings.DurationSeconds = 1;
                settings.IntervalMilliseconds = 500;

                if (CaptureService.CaptureBurst(target, settings, folder).Count < 1)
                {
                    return 3;
                }

                string[] frames = Directory.GetFiles(folder, "*.png");
                if (frames.Length < 1)
                {
                    return 4;
                }

                using (Bitmap frame = new Bitmap(frames[0]))
                {
                    if (frame.Width != probe.Width || frame.Height != probe.Height)
                    {
                        return 5;
                    }
                }

                return 0;
            }
            catch
            {
                return 1;
            }
            finally
            {
                if (!string.IsNullOrEmpty(folder) && Directory.Exists(folder))
                {
                    try
                    {
                        Directory.Delete(folder, true);
                    }
                    catch
                    {
                    }
                }
            }
        }
    }
}
