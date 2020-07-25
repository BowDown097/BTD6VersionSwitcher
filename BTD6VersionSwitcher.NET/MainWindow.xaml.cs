using Newtonsoft.Json;
using Ookii.Dialogs.Wpf;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Windows;
using System.Windows.Media;

namespace BTD6VersionSwitcher.NET
{
    public partial class MainWindow : Window
    {
        // this is all shit we're throwing into DepotDownloader later
        private static readonly string[] versions = new WebClient().DownloadString("https://dpoge.github.io/VersionSwitcher/versions.html").Replace("\n", "").Split(' '); // github likes inserting \n to the end of html files for some reason, so do .replace just in case
        private static readonly string[] manifests = new WebClient().DownloadString("https://dpoge.github.io/VersionSwitcher/manifests.html").Replace("\n", "").Split(' ');
        private static readonly string[] offsets = new WebClient().DownloadString("https://dpoge.github.io/VersionSwitcher/offsets.html").Replace("\n", "").Split(' ');
        private static readonly Dictionary<string, string> manifestDict = versions.Zip(manifests, (ver, man) => new { ver, man })
                                                              .ToDictionary(item => item.ver, item => item.man);
        private static readonly Dictionary<string, string> offsetDict = versions.Zip(offsets, (ver, off) => new { ver, off })
                                                              .ToDictionary(item => item.ver, item => item.off);

        public MainWindow()
        {
            InitializeComponent();
            // set up versionSelect ComboBox
            foreach (string version in versions)
            {
                versionSelect.Items.Add(version);
            }
            versionSelect.SelectedIndex = 0;
        }

        private void SwitchBtn_Click(object sender, RoutedEventArgs e)
        {
            string directory = btd6Folder.Text;
            if (directory != "No folder selected!" &&
                !Color.FromRgb(255, 0, 0).Equals((btd6Folder.Background as SolidColorBrush).Color)) // red = error when getting directory, don't want this
            {
                string version = versionSelect.Text;
                // trophy store warning (if version < 19.0)
                if (Convert.ToDouble(version) < 19.0)
                {
                    // get save location
                    string dir = "[Couldn't find dir]";
                    string userDataDir = SteamUtils.GetGameDataDir(960090);
                    if (!string.IsNullOrEmpty(userDataDir))
                    {
                        dir = Directory.EnumerateFiles(userDataDir, "Profile.Save", SearchOption.AllDirectories).First();
                    }
                    // show warning, return if cancelled
                    if (MessageBox.Show("Switching to a version before 19.0 WILL result in losing your Trophy Store purchases. If you have any, it is crucial that you back up your Profile.Save in \"" + dir + "\" or back up your save data using another method before switching.", "Important Warning", MessageBoxButton.OKCancel) == MessageBoxResult.Cancel)
                    {
                        return;
                    }
                }
                // get steam username/password
                InputDialog usernameDialog = new InputDialog("Please enter your Steam username. It is required to continue (this will not be stored anywhere! you can look at the source code if you wish)", "Steam username?");
                InputDialog passwordDialog = new InputDialog("Please enter your Steam password. It is required to continue (this will not be stored anywhere! you can look at the source code if you wish)", "Steam password?");
                if (usernameDialog.ShowDialog() == true && passwordDialog.ShowDialog() == true)
                {
                    console.Text += "\n\nDownloading version " + version + "...";
                    // download depot
                    if (Directory.Exists("depots"))
                    {
                        try
                        {
                            Directory.Delete("depots", true);
                        }
                        catch (IOException)
                        {
                            MessageBox.Show("Your depots directory is still opened! Make sure to close out of it in anything you have it open in.");
                        }
                    }
                    Process depotProc = new Process();
                    depotProc.StartInfo.FileName = @"DepotDownloader\depotdownloader.bat";
                    depotProc.StartInfo.Arguments = $"-app 960090 -depot 960091 -manifest {ulong.Parse(manifestDict[version])} -username {usernameDialog.Answer} -password {passwordDialog.Answer}";
                    depotProc.Start();
                    depotProc.WaitForExit();
                    string gameAssembly = Directory.EnumerateFiles(@"depots\960091", "GameAssembly.dll", SearchOption.AllDirectories).First();

                    if (!string.IsNullOrEmpty(gameAssembly))
                    {
                        // modify GameAssembly.dll
                        console.Text += "\nApplying patches...";
                        using (FileStream fileStream = new FileStream(gameAssembly, FileMode.Open))
                        {
                            using (BinaryWriter writer = new BinaryWriter(fileStream))
                            {
                                if (Convert.ToDouble(version) < 14.0)
                                {
                                    writer.Seek(int.Parse(offsetDict[version]), SeekOrigin.Begin);
                                    writer.Write(new byte[] { 0xC3 }, 0, 1);
                                } // 7.1-13.1 have a method TitleScreen.ShowForceUpdatePopup, which we can just return
                                else
                                {
                                    int[] offArr = JsonConvert.DeserializeObject<int[]>(offsetDict[version]);
                                    writer.Seek(offArr[0], SeekOrigin.Begin);
                                    writer.Write(new byte[] { 0x00 }, 0, 1);
                                    writer.Seek(offArr[1], SeekOrigin.Begin);
                                    writer.Write(new byte[] { 0x85 }, 0, 1);
                                } // 14.0+ merged this method with TitleScreen.UpdateVersion, so we have to do it a little bit differently
                            }
                        }
                        // copy new depot files over
                        console.Text += "\nMoving files...";
                        string source = Path.GetDirectoryName(gameAssembly);
                        foreach (string dirPath in Directory.GetDirectories(source, "*", SearchOption.AllDirectories))
                        {
                            Directory.CreateDirectory(dirPath.Replace(source, directory));
                        }
                        foreach (string newPath in Directory.GetFiles(source, "*.*", SearchOption.AllDirectories))
                        {
                            File.Copy(newPath, newPath.Replace(source, directory), true);
                        }
                        Directory.Delete(source, true);

                        console.Text += "\nDone!";
                    }
                    else
                    {
                        MessageBox.Show("A problem occurred! Either the version didn't download fully or you entered your Steam login details incorrectly. Try again!", "Error");
                    }
                }
            }
            else
            {
                MessageBox.Show("You haven't selected your BTD6 folder or the selected folder is invalid, please fix that!", "Warning");
            }
        }

        private void OpenFolderBtn_Click(object sender, RoutedEventArgs e)
        {
            string btd6dir = SteamUtils.GetGameDir(960090, "BloonsTD6");
            if (string.IsNullOrEmpty(btd6dir))
            {
                VistaFolderBrowserDialog dialog = new VistaFolderBrowserDialog
                {
                    Description = "Choose BTD6 directory",
                    UseDescriptionForTitle = true
                };

                if (dialog.ShowDialog() == true)
                {
                    btd6Folder.Text = dialog.SelectedPath.Replace("\\", "/");
                }
            } // if btd6dir is null, that means it failed to find btd6's steam directory - it will display a folder select dialog if it does
            else
            {
                btd6Folder.Text = btd6dir.Replace("\\", "/");
            }

            if (btd6Folder.Text.ToLower().Contains("steamapps/common/bloonstd6") && Directory.Exists(btd6Folder.Text + "/BloonsTD6_Data"))
            {
                btd6Folder.Background = new SolidColorBrush(Color.FromRgb(0, 255, 0)); // lime green
            }
            else
            {
                btd6Folder.Background = new SolidColorBrush(Color.FromRgb(255, 0, 0)); // red
            }
        }
    }
}