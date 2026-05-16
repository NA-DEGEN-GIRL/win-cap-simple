using System;
using System.Drawing;
using System.Drawing.Imaging;
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
        private readonly Button _closeButton = new Button();
        private Bitmap _clipboardImage;

        public Bitmap SelectedImage;

        public BurstViewerForm(string folder)
        {
            _folder = folder;

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

            buttons.Controls.Add(_closeButton);
            buttons.Controls.Add(_useButton);
            buttons.Controls.Add(_copyButton);
            buttons.Controls.Add(_saveButton);

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
