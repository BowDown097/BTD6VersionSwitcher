#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    QStringList getVersionInfo(const QString& which);
    void openBtd6Folder();
    void switchVersion();
    void joinDiscord();
    void downloadFile(const QString& url, const QString& fileLocation);
    void unzip(const std::string& file, const std::string& destination);

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
