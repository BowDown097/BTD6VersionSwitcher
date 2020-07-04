#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "miniz.h"
#include <sstream>
#include <filesystem>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <QFileDialog>
#include <QDir>
#include <QSysInfo>
#include <QMessageBox>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QDesktopServices>
#include <QFileInfo>
#include <QThread>
#include <QException>
#include <QSaveFile>
#include <QShortcut>

namespace fs = std::filesystem;

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds) // cross-platform sleep function
{
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(milliseconds * 1000);
#endif
}

QStringList MainWindow::getVersionInfo(const QString& which)
{
    // resolve url
    QString url = "https://dpoge.github.io/VersionSwitcher/" + which + ".html";
    QString kernel = QSysInfo::kernelType().toLower();
    if(kernel.contains("mac")) // if user is running mac
    {
        url = "https://dpoge.github.io/VersionSwitcher/" + which + "-mac.html";
    }
    else if(!kernel.contains("windows") && kernel != "linux") // not running windows, mac, or linux (prob not even possible, but better safe than sorry)
    {
        QMessageBox::warning(this, "Warning", "Your kernel, " + kernel + ", is not supported. However, the program will attempt to use the Windows versions. This may not work.");
    }
    // write versions from github.io page to string
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkReply* response = manager->get(QNetworkRequest(QUrl(url)));
    QEventLoop event;
    connect(response, SIGNAL(finished()), &event, SLOT(quit()));
    event.exec();
    QString content = response->readAll();
    // parse to QStringList
    std::stringstream ss(content.toStdString());
    std::string buf;
    QStringList result;
    while(ss >> buf)
        result << QString::fromStdString(buf);
    return result;
}

void MainWindow::downloadFile(const QString& url, const QString& fileLocation)
{
    try
    {
        QThread* thread = QThread::create([&]
        {
            // get bytes for zip
            QNetworkAccessManager* manager = new QNetworkAccessManager();
            QNetworkReply* response = manager->get(QNetworkRequest(QUrl(url)));
            QEventLoop event;
            connect(response, SIGNAL(finished()), &event, SLOT(quit()));
            event.exec();
            QByteArray responseData = response->readAll();
            // save bytes to zip file
            QSaveFile file(fileLocation);
            file.open(QIODevice::WriteOnly);
            file.write(responseData);
            file.commit();
        });
        thread->start();
        while(thread->isRunning())
        {
            sleep_ms(30);
            qApp->processEvents();
        }
    }
    catch (const QException& e)
    {
        QMessageBox::warning(this, "Error", "Error occurred downloading the file: " + QString::fromUtf8(e.what()));
    }
}

void MainWindow::unzip(const std::string& file, const std::string& destination)
{
    try
    {
        QThread* thread = QThread::create([&]
        {
            mz_zip_archive zip_archive;
            memset(&zip_archive, 0, sizeof(zip_archive));

            auto status = mz_zip_reader_init_file(&zip_archive, file.c_str(), 0);
            if(status)
            {
                int fileCount = (int)mz_zip_reader_get_num_files(&zip_archive);
                if(fileCount != 0)
                {
                    mz_zip_archive_file_stat file_stat;
                    if(mz_zip_reader_file_stat(&zip_archive, 0, &file_stat))
                    {
                        for(int i = 0; i < fileCount; i++)
                        {
                            if(!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
                            if(mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;
                            std::string destFile = fs::relative(destination + "/" + file_stat.m_filename);
                            QDir newDir = QDir(QString::fromStdString(fs::path(destFile).parent_path()));
                            if(!newDir.exists()) newDir.mkpath(".");
                            mz_zip_reader_extract_to_file(&zip_archive, i, destFile.c_str(), 0);
                            qApp->processEvents();
                        }
                    }
                }
            }
            mz_zip_reader_end(&zip_archive);
        });
        thread->start();
        while(thread->isRunning())
        {
            sleep_ms(30);
            qApp->processEvents();
        }
    }
    catch (const QException& e)
    {
        QMessageBox::warning(this, "Warning", "Error occurred unzipping the file: " + QString::fromUtf8(e.what()));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setWindowIcon(QIcon(":/icon.ico"));
    ui->setupUi(this);
    const QStringList versionInfo = getVersionInfo("versions");
    ui->versionSelect->addItems(versionInfo);
    // disable resizing
    this->statusBar()->setSizeGripEnabled(false);
    this->setFixedSize(QSize(800, 600));
    // ui shit
    connect(ui->openFolderBtn, SIGNAL(clicked()), this, SLOT(openBtd6Folder()));
    connect(ui->switchBtn, SIGNAL(clicked()), this, SLOT(switchVersion()));
    connect(ui->discordBtn, SIGNAL(clicked()), this, SLOT(joinDiscord()));
    // hotkeys
    QShortcut* refresh = new QShortcut(QKeySequence(Qt::Key_F5), this);
    QShortcut* clear = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    QShortcut* modded = new QShortcut(QKeySequence(Qt::Key_M), this);
    QObject::connect(refresh, &QShortcut::activated, this, &MainWindow::refreshVersions);
    QObject::connect(clear, &QShortcut::activated, this, &MainWindow::clearVersionCache);
    QObject::connect(modded, &QShortcut::activated, this, &MainWindow::moddedVersions);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openBtd6Folder()
{
    // attempt to make initial directory
    QString initialDir;
#ifdef WIN32
    if(QDir("C:/Program Files (x86)/Steam/steamapps/common/BloonsTD6").exists())
        initialDir = "C:/Program Files (x86)/Steam/steamapps/common/BloonsTD6";
#elif _POSIX_C_SOURCE >= 199309L
    // get home directory
    QString home;
    if((home = getenv("HOME")) == NULL)
        home = getpwuid(getuid())->pw_dir;
    // see if btd6 dir exists
    if(QDir(home + "/.steam/steam/steamapps/common/BloonsTD6").exists())
        initialDir = home + "/.steam/steam/steamapps/common/BloonsTD6";
#endif // sorry mac users nothing for you

    // create folder dialog, set as dir
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose BTD6 directory"), initialDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->btd6Folder->setText(dir);
    if(dir.toLower().contains("steamapps/common/bloonstd6") && QDir(dir + "/BloonsTD6_Data").exists())
    {
        ui->btd6Folder->setStyleSheet("QWidget { background-color: rgb(102, 255, 102); }");
        QDir versions(dir + "/versions");
        if(!versions.exists())
            versions.mkpath(".");
    }
    else
    {
        ui->btd6Folder->setStyleSheet("QWidget { background-color: red; }");
    }
}

QString downloads = "downloads";
void MainWindow::switchVersion()
{
    const QString directory = ui->btd6Folder->text();
    if(directory != "No folder selected!" && ui->btd6Folder->styleSheet() != "QWidget { background-color: red; }") // if user selected a valid directory
    {
        const QString version = ui->versionSelect->currentText();
        const QString zipPath = directory + "/versions/" + version + ".zip";
        const QStringList versionLinks = getVersionInfo(downloads);
        if(!QFileInfo(zipPath).exists())
        {
            ui->console->append("Downloading version " + version + "...");
            downloadFile(versionLinks.at(ui->versionSelect->currentIndex()), zipPath);
        }
        else
        {
            ui->console->append("Zip for " + version + " exists!");
        }
        ui->console->append("Extracting...");
        unzip(zipPath.toStdString(), directory.toStdString());
        ui->console->append("Done!\n");
        return;
    }
    QMessageBox::warning(this, "Warning", "You haven't selected your BTD6 folder or the selected folder is invalid, please fix that!");
}

void MainWindow::joinDiscord()
{
    QDesktopServices::openUrl(QUrl("https://discord.gg/MmnUCWV"));
}

void MainWindow::refreshVersions()
{
    if(QMessageBox::question(this, "Refresh versions list?", "F5 was pressed", QMessageBox::Yes| QMessageBox::No) == QMessageBox::Yes)
    {
        const QStringList versionsOverride = getVersionInfo("versions");
        ui->versionSelect->clear();
        ui->versionSelect->addItems(versionsOverride);
        QMessageBox::information(this, "Success", "Versions list was refreshed!");
    }
}

void MainWindow::clearVersionCache()
{
    if(QMessageBox::question(this, "Clear versions folder?", "Delete was pressed", QMessageBox::Yes| QMessageBox::No) == QMessageBox::Yes)
    {
        const QString path = ui->btd6Folder->text() + "/versions";
        QDir dir(path);
        if(dir.exists())
        {
            dir.setNameFilters(QStringList() << "*.*");
            dir.setFilter(QDir::Files);
            foreach(QString file, dir.entryList())
                dir.remove(file);
            QMessageBox::information(this, "Success", "Versions folder has been cleared!");
        }
        else
        {
            QMessageBox::warning(this, "Warning", "You haven't selected your BTD6 folder or the selected folder is invalid, please fix that!");
        }
    }
}

void MainWindow::moddedVersions()
{
    if(QMessageBox::question(this, "Use modded versions list?", "M was pressed", QMessageBox::Yes| QMessageBox::No) == QMessageBox::Yes)
    {
        const QStringList versionsOverride = getVersionInfo("versions-modded");
        downloads = "downloads-modded";
        ui->versionSelect->clear();
        ui->versionSelect->addItems(versionsOverride);
        QMessageBox::information(this, "Success", "Versions list was refreshed!");
    }
}
