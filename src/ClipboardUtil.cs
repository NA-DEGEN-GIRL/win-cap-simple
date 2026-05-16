using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal static class ClipboardUtil
    {
        public static void SetImage(Bitmap bitmap)
        {
            DataObject data = new DataObject();
            data.SetData(DataFormats.Bitmap, true, bitmap);

            ExternalException lastError = null;
            for (int attempt = 0; attempt < 6; attempt++)
            {
                try
                {
                    Clipboard.SetDataObject(data, true);
                    return;
                }
                catch (ExternalException ex)
                {
                    lastError = ex;
                    Thread.Sleep(25);
                }
            }

            if (lastError != null)
            {
                throw lastError;
            }

            throw new ExternalException("Could not set clipboard data.");
        }
    }
}
