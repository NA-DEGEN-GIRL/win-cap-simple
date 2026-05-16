using System;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal static class Program
    {
        [STAThread]
        [LoaderOptimization(LoaderOptimization.SingleDomain)]
        private static int Main(string[] args)
        {
            try
            {
                NativeMethods.SetProcessDPIAware();
            }
            catch
            {
                // Older Windows versions may not expose this API; capture still works.
            }

            if (args != null && args.Length > 0 && string.Equals(args[0], "--self-test", StringComparison.OrdinalIgnoreCase))
            {
                return SelfTest.Run();
            }

            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
            return 0;
        }
    }
}
