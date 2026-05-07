using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SolidWorksBatchConverter
{
    public partial class MainForm : Form
    {
        private string _inputDirectory;
        private string _outputDirectory;
        private SolidWorksBatchConverter _converter;
        private Logger _logger;

        public MainForm()
        {
            InitializeComponent();
            _outputDirectory = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "OUT");
            txtOutputDir.Text = _outputDirectory;
        }

        private void InitializeComponent()
        {
            this.SuspendLayout();
            
            this.Text = "SOLIDWORKS 批量 OBJ 转换器 v1.0";
            this.Size = new System.Drawing.Size(700, 500);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            var inputLabel = new Label
            {
                Text = "输入目录:",
                Location = new System.Drawing.Point(12, 12),
                Size = new System.Drawing.Size(75, 20)
            };

            var inputTextBox = new TextBox
            {
                Name = "txtInputDir",
                Location = new System.Drawing.Point(93, 12),
                Size = new System.Drawing.Size(450, 20),
                ReadOnly = true
            };

            var inputButton = new Button
            {
                Text = "浏览...",
                Location = new System.Drawing.Point(549, 10),
                Size = new System.Drawing.Size(75, 23)
            };
            inputButton.Click += btnInputBrowse_Click;

            var outputLabel = new Label
            {
                Text = "输出目录:",
                Location = new System.Drawing.Point(12, 38),
                Size = new System.Drawing.Size(75, 20)
            };

            var outputTextBox = new TextBox
            {
                Name = "txtOutputDir",
                Location = new System.Drawing.Point(93, 38),
                Size = new System.Drawing.Size(450, 20),
                ReadOnly = true
            };

            var outputButton = new Button
            {
                Text = "固定",
                Location = new System.Drawing.Point(549, 36),
                Size = new System.Drawing.Size(75, 23),
                Enabled = false
            };

            var currentFileLabel = new Label
            {
                Text = "当前处理:",
                Location = new System.Drawing.Point(12, 64),
                Size = new System.Drawing.Size(75, 20)
            };

            var currentFileTextBox = new TextBox
            {
                Name = "txtCurrentFile",
                Location = new System.Drawing.Point(93, 64),
                Size = new System.Drawing.Size(450, 20),
                ReadOnly = true
            };

            var progressBar = new ProgressBar
            {
                Name = "progressBar",
                Location = new System.Drawing.Point(12, 90),
                Size = new System.Drawing.Size(612, 23),
                Style = ProgressBarStyle.Continuous
            };

            var startButton = new Button
            {
                Name = "btnStartConvert",
                Text = "开始转换",
                Location = new System.Drawing.Point(549, 62),
                Size = new System.Drawing.Size(75, 23)
            };
            startButton.Click += btnStartConvert_Click;

            var cancelButton = new Button
            {
                Name = "btnCancel",
                Text = "取消",
                Location = new System.Drawing.Point(549, 90),
                Size = new System.Drawing.Size(75, 23),
                Enabled = false
            };
            cancelButton.Click += btnCancel_Click;

            var logLabel = new Label
            {
                Text = "日志:",
                Location = new System.Drawing.Point(12, 116),
                Size = new System.Drawing.Size(75, 20)
            };

            var logTextBox = new RichTextBox
            {
                Name = "txtLog",
                Location = new System.Drawing.Point(12, 138),
                Size = new System.Drawing.Size(676, 320),
                ReadOnly = true,
                Font = new System.Drawing.Font("Consolas", 9)
            };

            this.Controls.Add(inputLabel);
            this.Controls.Add(inputTextBox);
            this.Controls.Add(inputButton);
            this.Controls.Add(outputLabel);
            this.Controls.Add(outputTextBox);
            this.Controls.Add(outputButton);
            this.Controls.Add(currentFileLabel);
            this.Controls.Add(currentFileTextBox);
            this.Controls.Add(progressBar);
            this.Controls.Add(startButton);
            this.Controls.Add(cancelButton);
            this.Controls.Add(logLabel);
            this.Controls.Add(logTextBox);

            this.ResumeLayout(false);
        }

        private TextBox txtInputDir => (TextBox)Controls.Find("txtInputDir", true)[0];
        private TextBox txtOutputDir => (TextBox)Controls.Find("txtOutputDir", true)[0];
        private TextBox txtCurrentFile => (TextBox)Controls.Find("txtCurrentFile", true)[0];
        private ProgressBar progressBar => (ProgressBar)Controls.Find("progressBar", true)[0];
        private Button btnStartConvert => (Button)Controls.Find("btnStartConvert", true)[0];
        private Button btnCancel => (Button)Controls.Find("btnCancel", true)[0];
        private RichTextBox txtLog => (RichTextBox)Controls.Find("txtLog", true)[0];

        private void btnInputBrowse_Click(object sender, EventArgs e)
        {
            using (var folderDialog = new FolderBrowserDialog())
            {
                if (folderDialog.ShowDialog() == DialogResult.OK)
                {
                    _inputDirectory = folderDialog.SelectedPath;
                    txtInputDir.Text = _inputDirectory;
                }
            }
        }

        private void btnOutputBrowse_Click(object sender, EventArgs e)
        {
            using (var folderDialog = new FolderBrowserDialog())
            {
                if (folderDialog.ShowDialog() == DialogResult.OK)
                {
                    _outputDirectory = folderDialog.SelectedPath;
                    txtOutputDir.Text = _outputDirectory;
                }
            }
        }

        private async void btnStartConvert_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(_inputDirectory))
            {
                MessageBox.Show("请选择输入目录", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            _outputDirectory = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "OUT");
            txtOutputDir.Text = _outputDirectory;

            if (!Directory.Exists(_inputDirectory))
            {
                MessageBox.Show("输入目录不存在", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            try
            {
                Directory.CreateDirectory(_outputDirectory);
            }
            catch
            {
                MessageBox.Show("无法创建输出目录", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            _logger = new Logger(AppDomain.CurrentDomain.BaseDirectory, txtLog);
            _converter = new SolidWorksBatchConverter(_logger);
            _converter.FileProcessing += Converter_FileProcessing;
            _converter.ProgressChanged += Converter_ProgressChanged;

            btnStartConvert.Enabled = false;
            btnCancel.Enabled = true;
            progressBar.Value = 0;
            txtLog.Clear();

            // SOLIDWORKS COM 须在 STA 线程上使用；Task.Run 使用 MTA 线程池会导致调度/封送异常
            await RunOnStaThreadAsync(() =>
            {
                try
                {
                    if (!_converter.StartSolidWorks())
                    {
                        Invoke((Action)(() =>
                        {
                            MessageBox.Show("无法启动 SOLIDWORKS", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }));
                        return;
                    }

                    _converter.ConvertFolder(_inputDirectory, _outputDirectory);
                }
                catch (Exception ex)
                {
                    _logger.LogError($"批处理异常: {ex.Message}");
                }
                finally
                {
                    _converter.ShutdownSolidWorks();
                }
            });

            btnStartConvert.Enabled = true;
            btnCancel.Enabled = false;
            progressBar.Value = 100;

            MessageBox.Show("批量转换完成！", "完成", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            if (_converter != null)
            {
                _converter.Cancel();
            }
        }

        private void Converter_FileProcessing(object sender, string filePath)
        {
            if (txtCurrentFile.InvokeRequired)
            {
                txtCurrentFile.Invoke(new Action(() => txtCurrentFile.Text = Path.GetFileName(filePath)));
            }
            else
            {
                txtCurrentFile.Text = Path.GetFileName(filePath);
            }
        }

        private void Converter_ProgressChanged(object sender, int progress)
        {
            if (progressBar.InvokeRequired)
            {
                progressBar.Invoke(new Action(() => progressBar.Value = progress));
            }
            else
            {
                progressBar.Value = progress;
            }
        }

        /// <summary>在单独 STA 线程上执行 SOLIDWORKS COM 调用（与 ISldWorks 套间一致）。</summary>
        private static Task RunOnStaThreadAsync(Action action)
        {
            var tcs = new TaskCompletionSource<object>();
            var thread = new Thread(() =>
            {
                try
                {
                    action();
                    tcs.SetResult(null);
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            });
            thread.SetApartmentState(ApartmentState.STA);
            thread.Start();
            return tcs.Task;
        }
    }
}