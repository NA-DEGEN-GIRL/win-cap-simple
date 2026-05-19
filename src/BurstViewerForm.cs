using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class BurstViewerForm : Form
    {
        private readonly string _folder;
        private readonly ListBox _frames = new ListBox();
        private readonly PictureBox _preview = new PictureBox();
        private readonly Button _useButton = new Button();
        private readonly Button _copyButton = new Button();
        private readonly Button _saveButton = new Button();
        private readonly Button _mp4Button = new Button();
        private readonly Button _gifButton = new Button();
        private readonly Button _closeButton = new Button();
        private Bitmap _clipboardImage;
        private readonly int _intervalMilliseconds;

        public Bitmap SelectedImage;

        public BurstViewerForm(string folder)
        {
            _folder = folder;
            _intervalMilliseconds = ReadBurstIntervalMilliseconds(folder);

            Text = "Burst Viewer";
            StartPosition = FormStartPosition.CenterParent;
            Width = 980;
            Height = 680;
            MinimumSize = new Size(720, 480);

            SplitContainer split = new SplitContainer();
            split.Dock = DockStyle.Fill;
            split.FixedPanel = FixedPanel.Panel1;
            split.SplitterDistance = 190;

            _frames.Dock = DockStyle.Fill;
            _frames.IntegralHeight = false;
            _frames.SelectedIndexChanged += delegate { UpdatePreview(); };
            split.Panel1.Controls.Add(_frames);

            _preview.Dock = DockStyle.Fill;
            _preview.SizeMode = PictureBoxSizeMode.Zoom;
            _preview.BackColor = Color.FromArgb(245, 246, 248);
            split.Panel2.Controls.Add(_preview);

            FlowLayoutPanel buttons = new FlowLayoutPanel();
            buttons.Dock = DockStyle.Bottom;
            buttons.Height = 44;
            buttons.FlowDirection = FlowDirection.RightToLeft;
            buttons.Padding = new Padding(8);

            _closeButton.Text = "Close";
            _closeButton.Size = new Size(84, 28);
            _closeButton.DialogResult = DialogResult.Cancel;

            _useButton.Text = "Use";
            _useButton.Size = new Size(84, 28);
            _useButton.Click += delegate { UseSelected(); };

            _copyButton.Text = "Copy";
            _copyButton.Size = new Size(84, 28);
            _copyButton.Click += delegate { CopySelected(); };

            _saveButton.Text = "Save As";
            _saveButton.Size = new Size(84, 28);
            _saveButton.Click += delegate { SaveSelected(); };

            _mp4Button.Text = "MP4";
            _mp4Button.Size = new Size(84, 28);
            _mp4Button.Click += delegate { ExportBurst(true); };

            _gifButton.Text = "GIF";
            _gifButton.Size = new Size(84, 28);
            _gifButton.Click += delegate { ExportBurst(false); };

            buttons.Controls.Add(_closeButton);
            buttons.Controls.Add(_useButton);
            buttons.Controls.Add(_copyButton);
            buttons.Controls.Add(_saveButton);
            buttons.Controls.Add(_mp4Button);
            buttons.Controls.Add(_gifButton);

            Controls.Add(split);
            Controls.Add(buttons);

            LoadFrames();
            if (_frames.Items.Count > 0)
            {
                _frames.SelectedIndex = 0;
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_preview.Image != null)
                {
                    _preview.Image.Dispose();
                    _preview.Image = null;
                }

                if (_clipboardImage != null)
                {
                    _clipboardImage.Dispose();
                    _clipboardImage = null;
                }
            }

            base.Dispose(disposing);
        }

        private void LoadFrames()
        {
            if (string.IsNullOrEmpty(_folder) || !Directory.Exists(_folder))
            {
                return;
            }

            string[] files = Directory.GetFiles(_folder, "*.png");
            Array.Sort(files, StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < files.Length; i++)
            {
                _frames.Items.Add(new FrameItem(files[i]));
            }
        }

        private void UpdatePreview()
        {
            string file = GetSelectedFile();
            if (file == null)
            {
                return;
            }

            if (_preview.Image != null)
            {
                _preview.Image.Dispose();
                _preview.Image = null;
            }

            _preview.Image = LoadBitmapUnlocked(file);
            _frames.HorizontalScrollbar = true;
        }

        private void UseSelected()
        {
            string file = GetSelectedFile();
            if (file == null)
            {
                return;
            }

            SelectedImage = LoadBitmapUnlocked(file);
            DialogResult = DialogResult.OK;
            Close();
        }

        private void CopySelected()
        {
            string file = GetSelectedFile();
            Bitmap nextClipboardImage;
            if (file == null)
            {
                return;
            }

            nextClipboardImage = LoadBitmapUnlocked(file);
            try
            {
                ClipboardUtil.SetImage(nextClipboardImage);
                if (_clipboardImage != null)
                {
                    _clipboardImage.Dispose();
                }
                _clipboardImage = nextClipboardImage;
                nextClipboardImage = null;
            }
            finally
            {
                if (nextClipboardImage != null)
                {
                    nextClipboardImage.Dispose();
                }
            }
        }

        private void SaveSelected()
        {
            string file = GetSelectedFile();
            if (file == null)
            {
                return;
            }

            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = "PNG image (*.png)|*.png";
                dialog.FileName = Path.GetFileName(file);
                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    using (Bitmap bitmap = LoadBitmapUnlocked(file))
                    {
                        bitmap.Save(dialog.FileName, ImageFormat.Png);
                    }
                }
            }
        }

        private void ExportBurst(bool mp4)
        {
            if (_frames.Items.Count == 0)
            {
                return;
            }

            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = mp4 ? "MP4 video (*.mp4)|*.mp4" : "GIF image (*.gif)|*.gif";
                dialog.DefaultExt = mp4 ? "mp4" : "gif";
                dialog.FileName = mp4 ? "burst.mp4" : "burst.gif";
                if (dialog.ShowDialog(this) != DialogResult.OK)
                {
                    return;
                }

                string oldTitle = Text;
                try
                {
                    Text = mp4 ? "Burst Viewer - exporting MP4" : "Burst Viewer - exporting GIF";
                    UseWaitCursor = true;
                    Application.DoEvents();
                    RunFfmpeg(mp4, dialog.FileName);
                    Text = mp4 ? "Burst Viewer - MP4 exported" : "Burst Viewer - GIF exported";
                }
                catch (Exception ex)
                {
                    Text = oldTitle;
                    MessageBox.Show(this, ex.Message, "Export failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                finally
                {
                    UseWaitCursor = false;
                }
            }
        }

        private void RunFfmpeg(bool mp4, string outputFile)
        {
            string pattern = Path.Combine(_folder, "frame_%04d.png");
            string arguments = "-y -hide_banner -loglevel error -framerate 1000/" +
                _intervalMilliseconds.ToString(CultureInfo.InvariantCulture) +
                " -i " + QuoteArg(pattern);

            if (mp4)
            {
                arguments += " -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" -c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p -movflags +faststart ";
            }
            else
            {
                arguments += " -loop 0 ";
            }

            arguments += QuoteArg(outputFile);

            ProcessStartInfo info = new ProcessStartInfo("ffmpeg.exe", arguments);
            info.UseShellExecute = false;
            info.CreateNoWindow = true;

            using (Process process = Process.Start(info))
            {
                process.WaitForExit();
                if (process.ExitCode != 0)
                {
                    throw new InvalidOperationException("ffmpeg failed to encode the burst.");
                }
            }
        }

        private static string QuoteArg(string value)
        {
            return "\"" + value.Replace("\"", "\\\"") + "\"";
        }

        private static int ReadBurstIntervalMilliseconds(string folder)
        {
            int interval;
            string file = Path.Combine(folder, "interval_ms.txt");
            if (File.Exists(file) &&
                int.TryParse(File.ReadAllText(file).Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out interval) &&
                interval >= 50)
            {
                return interval;
            }

            return 250;
        }

        private string GetSelectedFile()
        {
            FrameItem item = _frames.SelectedItem as FrameItem;
            return item == null ? null : item.FilePath;
        }

        private static Bitmap LoadBitmapUnlocked(string file)
        {
            using (FileStream stream = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (Image image = Image.FromStream(stream))
            {
                return new Bitmap(image);
            }
        }

        private sealed class FrameItem
        {
            public readonly string FilePath;

            public FrameItem(string filePath)
            {
                FilePath = filePath;
            }

            public override string ToString()
            {
                return Path.GetFileName(FilePath);
            }
        }
    }
}
