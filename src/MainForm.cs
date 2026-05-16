using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class MainForm : Form
    {
        private readonly AnnotatingCanvas _canvas = new AnnotatingCanvas();
        private readonly ToolStripComboBox _modeCombo = new ToolStripComboBox();
        private readonly ToolStripComboBox _displayCombo = new ToolStripComboBox();
        private readonly ToolStripComboBox _delayCombo = new ToolStripComboBox();
        private readonly ToolStripComboBox _widthCombo = new ToolStripComboBox();
        private readonly ToolStripButton _markerButton = new ToolStripButton("Marker");
        private readonly ToolStripStatusLabel _status = new ToolStripStatusLabel();

        private Bitmap _clipboardImage;
        private Icon _windowIcon;
        private string _lastBurstFolder;
        private bool _displaysLoaded;

        public MainForm()
        {
            Text = "WinCap Simple";
            Width = 1040;
            Height = 720;
            MinimumSize = new Size(760, 500);
            KeyPreview = true;

            SuspendLayout();
            Controls.Add(_canvas);
            Controls.Add(CreateStatusStrip());
            Controls.Add(CreateToolStrip());
            ResumeLayout(false);

            SetStatus("Ready.");
            BeginInvoke(new MethodInvoker(ApplyWindowIcon));
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == (Keys.Control | Keys.C))
            {
                CopyCurrent(true);
                return true;
            }

            if (keyData == (Keys.Control | Keys.N))
            {
                RunSingleCapture();
                return true;
            }

            return base.ProcessCmdKey(ref msg, keyData);
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            if (_clipboardImage != null)
            {
                _clipboardImage.Dispose();
                _clipboardImage = null;
            }

            if (_windowIcon != null)
            {
                _windowIcon.Dispose();
                _windowIcon = null;
            }

            base.OnFormClosed(e);
        }

        private void ApplyWindowIcon()
        {
            try
            {
                _windowIcon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
                if (_windowIcon != null)
                {
                    Icon = _windowIcon;
                }
            }
            catch
            {
            }
        }

        private ToolStrip CreateToolStrip()
        {
            ToolStrip toolStrip = new ToolStrip();
            toolStrip.GripStyle = ToolStripGripStyle.Hidden;
            toolStrip.Dock = DockStyle.Top;
            toolStrip.Padding = new Padding(6, 4, 6, 4);

            ToolStripButton newButton = new ToolStripButton("New Capture");
            newButton.Click += delegate { RunSingleCapture(); };

            _modeCombo.DropDownStyle = ComboBoxStyle.DropDownList;
            _modeCombo.Width = 116;
            _modeCombo.Items.Add("Rectangle");
            _modeCombo.Items.Add("Freeform");
            _modeCombo.Items.Add("Window");
            _modeCombo.Items.Add("Display");
            _modeCombo.SelectedIndex = 0;

            _displayCombo.DropDownStyle = ComboBoxStyle.DropDownList;
            _displayCombo.Width = 170;
            _displayCombo.Items.Add("Load on use");
            _displayCombo.SelectedIndex = 0;
            _displayCombo.DropDown += delegate { EnsureDisplaysPopulated(); };

            _delayCombo.DropDownStyle = ComboBoxStyle.DropDownList;
            _delayCombo.Width = 78;
            _delayCombo.Items.Add(new DelayItem("No delay", 0));
            _delayCombo.Items.Add(new DelayItem("1 sec", 1000));
            _delayCombo.Items.Add(new DelayItem("3 sec", 3000));
            _delayCombo.Items.Add(new DelayItem("5 sec", 5000));
            _delayCombo.Items.Add(new DelayItem("10 sec", 10000));
            _delayCombo.SelectedIndex = 0;

            ToolStripButton burstButton = new ToolStripButton("Burst");
            burstButton.Click += delegate { RunBurstCapture(); };

            ToolStripButton viewBurstButton = new ToolStripButton("View Burst");
            viewBurstButton.Click += delegate { OpenLatestBurstViewer(); };

            ToolStripButton copyButton = new ToolStripButton("Copy");
            copyButton.Click += delegate { CopyCurrent(true); };

            ToolStripButton saveButton = new ToolStripButton("Save");
            saveButton.Click += delegate { SaveCurrent(); };

            _markerButton.CheckOnClick = true;
            _markerButton.CheckedChanged += delegate
            {
                _canvas.MarkerEnabled = _markerButton.Checked;
                _canvas.Cursor = _markerButton.Checked ? Cursors.Cross : Cursors.Default;
                SetStatus(_markerButton.Checked ? "Marker enabled." : "Marker disabled.");
            };

            ToolStripDropDownButton colorButton = new ToolStripDropDownButton("Color");
            AddColorItem(colorButton, "Red", Color.FromArgb(232, 43, 43));
            AddColorItem(colorButton, "Yellow", Color.FromArgb(245, 190, 32));
            AddColorItem(colorButton, "Blue", Color.FromArgb(36, 111, 214));
            AddColorItem(colorButton, "Black", Color.FromArgb(20, 22, 25));

            _widthCombo.DropDownStyle = ComboBoxStyle.DropDownList;
            _widthCombo.Width = 58;
            _widthCombo.Items.Add("3 px");
            _widthCombo.Items.Add("6 px");
            _widthCombo.Items.Add("10 px");
            _widthCombo.Items.Add("16 px");
            _widthCombo.SelectedIndex = 1;
            _widthCombo.SelectedIndexChanged += delegate { UpdateMarkerWidth(); };

            ToolStripButton clearButton = new ToolStripButton("Clear Marks");
            clearButton.Click += delegate
            {
                _canvas.ClearMarks();
                SetStatus("Marks cleared.");
            };

            toolStrip.Items.Add(newButton);
            toolStrip.Items.Add(new ToolStripSeparator());
            toolStrip.Items.Add(new ToolStripLabel("Mode"));
            toolStrip.Items.Add(_modeCombo);
            toolStrip.Items.Add(new ToolStripLabel("Display"));
            toolStrip.Items.Add(_displayCombo);
            toolStrip.Items.Add(new ToolStripLabel("Delay"));
            toolStrip.Items.Add(_delayCombo);
            toolStrip.Items.Add(new ToolStripSeparator());
            toolStrip.Items.Add(burstButton);
            toolStrip.Items.Add(viewBurstButton);
            toolStrip.Items.Add(new ToolStripSeparator());
            toolStrip.Items.Add(copyButton);
            toolStrip.Items.Add(saveButton);
            toolStrip.Items.Add(new ToolStripSeparator());
            toolStrip.Items.Add(_markerButton);
            toolStrip.Items.Add(colorButton);
            toolStrip.Items.Add(new ToolStripLabel("Width"));
            toolStrip.Items.Add(_widthCombo);
            toolStrip.Items.Add(clearButton);

            return toolStrip;
        }

        private StatusStrip CreateStatusStrip()
        {
            StatusStrip strip = new StatusStrip();
            strip.Dock = DockStyle.Bottom;
            _status.Spring = true;
            _status.TextAlign = ContentAlignment.MiddleLeft;
            strip.Items.Add(_status);
            return strip;
        }

        private void AddColorItem(ToolStripDropDownButton button, string label, Color color)
        {
            ToolStripMenuItem item = new ToolStripMenuItem(label);
            item.BackColor = color;
            item.ForeColor = color.GetBrightness() < 0.5 ? Color.White : Color.Black;
            item.Click += delegate
            {
                _canvas.MarkerColor = color;
                SetStatus("Marker color: " + label + ".");
            };
            button.DropDownItems.Add(item);
        }

        private void EnsureDisplaysPopulated()
        {
            if (_displaysLoaded)
            {
                return;
            }

            _displayCombo.Items.Clear();
            Screen[] screens = Screen.AllScreens;
            for (int i = 0; i < screens.Length; i++)
            {
                _displayCombo.Items.Add(new ScreenItem(i, screens[i].Bounds, screens[i].Primary));
            }

            if (_displayCombo.Items.Count > 0)
            {
                _displayCombo.SelectedIndex = 0;
            }

            _displaysLoaded = true;
        }

        private void UpdateMarkerWidth()
        {
            string selected = _widthCombo.SelectedItem as string;
            if (selected == null)
            {
                return;
            }

            if (selected.StartsWith("3")) _canvas.MarkerWidth = 3.0f;
            if (selected.StartsWith("6")) _canvas.MarkerWidth = 6.0f;
            if (selected.StartsWith("10")) _canvas.MarkerWidth = 10.0f;
            if (selected.StartsWith("16")) _canvas.MarkerWidth = 16.0f;
        }

        private CaptureMode GetSelectedMode()
        {
            switch (_modeCombo.SelectedIndex)
            {
                case 1:
                    return CaptureMode.Freeform;
                case 2:
                    return CaptureMode.Window;
                case 3:
                    return CaptureMode.Display;
                default:
                    return CaptureMode.Rectangle;
            }
        }

        private int GetDelayMilliseconds()
        {
            DelayItem item = _delayCombo.SelectedItem as DelayItem;
            return item == null ? 0 : item.Milliseconds;
        }

        private void RunSingleCapture()
        {
            CaptureMode mode = GetSelectedMode();
            int delayMilliseconds = GetDelayMilliseconds();
            bool hidden = false;

            try
            {
                CaptureTarget target = PrepareTarget(mode, delayMilliseconds, out hidden);
                if (target == null)
                {
                    SetStatus("Capture canceled.");
                    return;
                }

                Bitmap bitmap = CaptureService.CaptureTarget(target);
                ShowAfterCapture(ref hidden);
                SetCapturedImage(bitmap, target.Description);
            }
            catch (Exception ex)
            {
                ShowAfterCapture(ref hidden);
                MessageBox.Show(this, ex.Message, "Capture failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SetStatus("Capture failed.");
            }
            finally
            {
                ShowAfterCapture(ref hidden);
            }
        }

        private void RunBurstCapture()
        {
            BurstSettings settings;
            using (BurstSettingsDialog dialog = new BurstSettingsDialog())
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                {
                    return;
                }
                settings = dialog.Settings;
            }

            CaptureMode mode = GetSelectedMode();
            int delayMilliseconds = GetDelayMilliseconds();
            bool hidden = false;

            try
            {
                CaptureTarget target = PrepareTarget(mode, delayMilliseconds, out hidden);
                if (target == null)
                {
                    SetStatus("Burst canceled.");
                    return;
                }

                string folder = CaptureService.CreateBurstFolder();
                List<string> files = CaptureService.CaptureBurst(target, settings, folder);
                _lastBurstFolder = folder;
                ShowAfterCapture(ref hidden);
                SetStatus("Burst saved " + files.Count + " frames.");
                OpenBurstViewer(folder);
            }
            catch (Exception ex)
            {
                ShowAfterCapture(ref hidden);
                MessageBox.Show(this, ex.Message, "Burst failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SetStatus("Burst failed.");
            }
            finally
            {
                ShowAfterCapture(ref hidden);
            }
        }

        private CaptureTarget PrepareTarget(CaptureMode mode, int delayMilliseconds, out bool hidden)
        {
            hidden = false;

            if (mode == CaptureMode.Window)
            {
                WindowInfo window = PickWindow();
                if (window == null)
                {
                    return null;
                }

                HideForCapture(ref hidden);
                WaitDelay(delayMilliseconds);

                CaptureTarget target = new CaptureTarget();
                target.Mode = CaptureMode.Window;
                target.WindowHandle = window.Handle;
                target.Bounds = window.Bounds;
                target.Description = "Window: " + window.Title;
                return target;
            }

            if (mode == CaptureMode.Display)
            {
                CaptureTarget target = GetDisplayTarget();
                HideForCapture(ref hidden);
                WaitDelay(delayMilliseconds);
                return target;
            }

            HideForCapture(ref hidden);
            WaitDelay(delayMilliseconds);

            using (SelectionOverlay overlay = new SelectionOverlay(mode))
            {
                if (overlay.ShowDialog() != DialogResult.OK)
                {
                    return null;
                }

                Application.DoEvents();
                Thread.Sleep(80);

                CaptureTarget target = new CaptureTarget();
                target.Mode = mode;
                target.Bounds = overlay.SelectedBounds;
                target.FreeformPoints = overlay.SelectedFreeformPoints;
                target.Description = mode == CaptureMode.Freeform ? "Freeform" : "Rectangle";
                return target;
            }
        }

        private WindowInfo PickWindow()
        {
            List<WindowInfo> windows = NativeMethods.GetVisibleWindows();
            using (WindowPickerDialog dialog = new WindowPickerDialog(windows))
            {
                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    return dialog.SelectedWindow;
                }
            }

            return null;
        }

        private CaptureTarget GetDisplayTarget()
        {
            EnsureDisplaysPopulated();

            ScreenItem item = _displayCombo.SelectedItem as ScreenItem;
            if (item == null)
            {
                Screen primary = Screen.PrimaryScreen;
                item = new ScreenItem(0, primary.Bounds, true);
            }

            CaptureTarget target = new CaptureTarget();
            target.Mode = CaptureMode.Display;
            target.Bounds = item.Bounds;
            target.Description = item.ToString();
            return target;
        }

        private void HideForCapture(ref bool hidden)
        {
            if (!hidden)
            {
                Hide();
                hidden = true;
                Application.DoEvents();
                NativeMethods.FlushDesktop();
                Thread.Sleep(20);
            }
        }

        private void ShowAfterCapture(ref bool hidden)
        {
            if (hidden)
            {
                Show();
                WindowState = FormWindowState.Normal;
                Activate();
                hidden = false;
            }
        }

        private void WaitDelay(int milliseconds)
        {
            if (milliseconds > 0)
            {
                Thread.Sleep(milliseconds);
            }
        }

        private void SetCapturedImage(Bitmap bitmap, string description)
        {
            _canvas.SetImage(bitmap);
            SetStatus("Captured " + description + ". Press Ctrl+C to copy.");
        }

        private void CopyCurrent(bool userVisible)
        {
            Bitmap nextClipboardImage = null;
            if (!_canvas.HasImage)
            {
                if (userVisible)
                {
                    SetStatus("Nothing to copy.");
                }
                return;
            }

            try
            {
                nextClipboardImage = _canvas.CreateFlattenedImage();
                ClipboardUtil.SetImage(nextClipboardImage);

                if (_clipboardImage != null)
                {
                    _clipboardImage.Dispose();
                }

                _clipboardImage = nextClipboardImage;
                nextClipboardImage = null;
                if (userVisible)
                {
                    SetStatus("Copied to clipboard.");
                }
            }
            catch (Exception ex)
            {
                if (userVisible)
                {
                    MessageBox.Show(this, ex.Message, "Copy failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                SetStatus("Copy failed.");
            }
            finally
            {
                if (nextClipboardImage != null)
                {
                    nextClipboardImage.Dispose();
                }
            }
        }

        private void SaveCurrent()
        {
            if (!_canvas.HasImage)
            {
                SetStatus("Nothing to save.");
                return;
            }

            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = "PNG image (*.png)|*.png";
                dialog.FileName = "capture_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".png";
                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    using (Bitmap bitmap = _canvas.CreateFlattenedImage())
                    {
                        bitmap.Save(dialog.FileName, ImageFormat.Png);
                    }
                    SetStatus("Saved.");
                }
            }
        }

        private void OpenLatestBurstViewer()
        {
            string folder = _lastBurstFolder;
            if (string.IsNullOrEmpty(folder) || !Directory.Exists(folder))
            {
                folder = CaptureService.FindLatestBurstFolder();
            }

            if (string.IsNullOrEmpty(folder))
            {
                SetStatus("No burst captures found.");
                return;
            }

            OpenBurstViewer(folder);
        }

        private void OpenBurstViewer(string folder)
        {
            using (BurstViewerForm viewer = new BurstViewerForm(folder))
            {
                if (viewer.ShowDialog(this) == DialogResult.OK && viewer.SelectedImage != null)
                {
                    SetCapturedImage(viewer.SelectedImage, "burst frame");
                    viewer.SelectedImage = null;
                }
            }
        }

        private void SetStatus(string message)
        {
            _status.Text = message;
        }
    }
}
