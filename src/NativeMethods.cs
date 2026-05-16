using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;

namespace WinCapSimple
{
    internal static class NativeMethods
    {
        private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        private const int Srccopy = 0x00CC0020;
        private const int CaptureBlt = 0x40000000;

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        public static extern bool SetProcessDPIAware();

        [DllImport("user32.dll")]
        private static extern IntPtr GetDC(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern int ReleaseDC(IntPtr hWnd, IntPtr hdc);

        [DllImport("gdi32.dll", SetLastError = true)]
        private static extern bool BitBlt(
            IntPtr hdcDest,
            int nXDest,
            int nYDest,
            int nWidth,
            int nHeight,
            IntPtr hdcSrc,
            int nXSrc,
            int nYSrc,
            int dwRop);

        [DllImport("dwmapi.dll", PreserveSig = true)]
        private static extern int DwmFlush();

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool IsIconic(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern int GetWindowTextLength(IntPtr hWnd);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        public static List<WindowInfo> GetVisibleWindows()
        {
            List<WindowInfo> windows = new List<WindowInfo>();
            uint currentPid = (uint)Process.GetCurrentProcess().Id;

            EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
            {
                if (!IsWindowVisible(hWnd) || IsIconic(hWnd))
                {
                    return true;
                }

                uint pid;
                GetWindowThreadProcessId(hWnd, out pid);
                if (pid == currentPid)
                {
                    return true;
                }

                int length = GetWindowTextLength(hWnd);
                if (length <= 0)
                {
                    return true;
                }

                Rectangle bounds;
                if (!TryGetWindowRectangle(hWnd, out bounds) || bounds.Width < 32 || bounds.Height < 32)
                {
                    return true;
                }

                StringBuilder builder = new StringBuilder(length + 1);
                GetWindowText(hWnd, builder, builder.Capacity);
                string title = builder.ToString().Trim();
                if (title.Length == 0)
                {
                    return true;
                }

                WindowInfo info = new WindowInfo();
                info.Handle = hWnd;
                info.Title = title;
                info.Bounds = bounds;
                windows.Add(info);
                return true;
            }, IntPtr.Zero);

            windows.Sort(delegate(WindowInfo left, WindowInfo right)
            {
                return string.Compare(left.Title, right.Title, StringComparison.CurrentCultureIgnoreCase);
            });

            return windows;
        }

        public static bool TryGetWindowRectangle(IntPtr handle, out Rectangle rectangle)
        {
            RECT rect;
            if (!GetWindowRect(handle, out rect))
            {
                rectangle = Rectangle.Empty;
                return false;
            }

            rectangle = Rectangle.FromLTRB(rect.Left, rect.Top, rect.Right, rect.Bottom);
            return rectangle.Width > 0 && rectangle.Height > 0;
        }

        public static IntPtr AcquireDesktopHdc()
        {
            IntPtr sourceHdc = GetDC(IntPtr.Zero);
            if (sourceHdc == IntPtr.Zero)
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not acquire the desktop device context.");
            }

            return sourceHdc;
        }

        public static void ReleaseDesktopHdc(IntPtr sourceHdc)
        {
            if (sourceHdc != IntPtr.Zero)
            {
                ReleaseDC(IntPtr.Zero, sourceHdc);
            }
        }

        public static void CopyScreenToHdc(IntPtr destinationHdc, int sourceX, int sourceY, int width, int height)
        {
            IntPtr sourceHdc = AcquireDesktopHdc();
            try
            {
                CopyDesktopHdcToHdc(sourceHdc, destinationHdc, sourceX, sourceY, width, height);
            }
            finally
            {
                ReleaseDesktopHdc(sourceHdc);
            }
        }

        public static void CopyDesktopHdcToHdc(IntPtr sourceHdc, IntPtr destinationHdc, int sourceX, int sourceY, int width, int height)
        {
            if (!BitBlt(destinationHdc, 0, 0, width, height, sourceHdc, sourceX, sourceY, Srccopy | CaptureBlt))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Desktop BitBlt failed.");
            }
        }

        public static void FlushDesktop()
        {
            try
            {
                DwmFlush();
            }
            catch
            {
            }
        }
    }
}
