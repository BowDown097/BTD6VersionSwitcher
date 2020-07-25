using System;
using System.Windows;

namespace BTD6VersionSwitcher.NET
{
    public partial class InputDialog : Window
    {
        public InputDialog(string question, string title = "")
        {
            InitializeComponent();
            lblQuestion.Content = question;
            Title = title;
        }

        private void BtnDialogOk_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = true;
        }

        private void Window_ContentRendered(object sender, EventArgs e)
        {
            txtAnswer.SelectAll();
            txtAnswer.Focus();
        }

        public string Answer => txtAnswer.Password;
    }
}