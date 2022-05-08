using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace CDZeroTrackRipper
{
    public partial class MainGUI : Form
    {
        public MainGUI()
        {
            InitializeComponent();
        }

        [DllImport("ripper.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int RipCD(char driveDir, string saveDir);

        enum Status
        {
            UnknownError = -5,
            Extracting = -4,
            Reading = -3,
            ExtractFailed = -2,
            NoTrackZero = -1
        }

        private void startRipButton_Click(object sender, EventArgs e)
        {
            if (sourceDirectoryComboBox.SelectedIndex != 0)
            {
                UpdateStatusBar((int) Status.Reading, false);
                int errorCode = 0;
                char driveDirectory = sourceDirectoryComboBox.Text[0];
                string saveDirectory = saveDirectoryTextBox.Text;

                try
                {
                    UpdateStatusBar((int)(Status.Extracting), false);
                    errorCode = RipCD(driveDirectory, saveDirectory);
                    UpdateStatusBar(errorCode, true);
                }
                catch
                {
                    UpdateStatusBar((int) Status.UnknownError, true);
                }
            }
            else
            {
                MessageBox.Show("Please select a drive.");
            }


        }

        private async Task UpdateStatusBar(int errorCode, bool resetStatusBar)
        {
            switch (errorCode)
            {
                case -5:
                    statusLabel.Text = "An unknown error has occured.";
                    break;
                case -4:
                    statusLabel.Text = "Extracting track...";
                    break;
                case -3:
                    statusLabel.Text = "Reading disc...";
                    break;
                case -2:
                    statusLabel.Text = "Unable to extract track.";
                    break;
                case -1:
                    statusLabel.Text = "There is no Track 0.";
                    break;
                case 0:
                    statusLabel.Text = "Successfully ripped Track 0.";
                    break;
                case 2:
                    statusLabel.Text = "The system cannot find the drive specified.";
                    break;
                case 5:
                    statusLabel.Text = "Access denied. Please make sure you specified the correct drive letter.";
                    break;
                case 21:
                    statusLabel.Text = "No disc.";
                    break;
            }
            if (resetStatusBar)
            {
                await Task.Delay(5000);
                statusLabel.Text = "Ready";
            }
        }

        private void browseButton_Click(object sender, EventArgs e)
        {
            if (saveFileDialog.ShowDialog() == DialogResult.OK)
            {
                saveDirectoryTextBox.Text = saveFileDialog.FileName;
                saveFileDialog.FileName = saveFileDialog.FileName.Remove(0, saveFileDialog.FileName.LastIndexOf("\\") + 1); //Removes the directory before the actual file name
            }
        }

        private void MainGUI_Load(object sender, EventArgs e)
        {
            sourceDirectoryComboBox.SelectedIndex = 0;
            DriveInfo[] drives = DriveInfo.GetDrives();

            foreach (DriveInfo drive in drives)
            {
                if (drive.IsReady)
                    sourceDirectoryComboBox.Items.Add(drive.Name + " " + drive.VolumeLabel);
                else
                    sourceDirectoryComboBox.Items.Add(drive.Name + " " + drive.DriveType);
            }
            //Debug only code
            #if DEBUG
            sourceDirectoryComboBox.SelectedIndex = 3;
            saveFileDialog.FileName = "G:\\Alex\\Documents\\CD\\test.wav";
            saveDirectoryTextBox.Text = saveFileDialog.FileName;
            saveFileDialog.FileName = saveFileDialog.FileName.Remove(0, saveFileDialog.FileName.LastIndexOf("\\") + 1);
            #endif
        }
    }
}
