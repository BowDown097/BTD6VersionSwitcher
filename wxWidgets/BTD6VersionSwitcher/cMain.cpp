#include "cMain.h"
#include "Poco/Environment.h"
#include "Poco/Path.h"
#include "Poco/StreamCopier.h"
#include "Poco/URI.h"
#include "Poco/URIStreamOpener.h"
#include "Poco/Net/AcceptCertificateHandler.h"
#include "Poco/Net/Context.h"
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Net/InvalidCertificateHandler.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/Zip/Decompress.h"
#include <memory>
#include <sstream>
#include <experimental/filesystem>
#include <fstream>
#include <ios>
#include <thread>
#include <atomic>
#pragma comment(lib, "Crypt32.lib")

#define wxGREY_BACKGROUND wxColour(52, 55, 60)
#define wxGREY_FOREGROUND wxColour(142, 146, 151)

namespace fs = std::experimental::filesystem;

void initializeSSL()
{
	Poco::Net::initializeSSL();
	Poco::Net::SSLManager::InvalidCertificateHandlerPtr ptrHandler(new Poco::Net::AcceptCertificateHandler(false));
	Poco::Net::Context::Ptr ptrContext(new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, ""));
	Poco::Net::SSLManager::instance().initializeClient(0, ptrHandler, ptrContext);
}

wxArrayString getVersionInfo(const std::string& which)
{
	std::string url = "https://dpoge.github.io/VersionSwitcher/" + which + ".html";
	if (Poco::Environment::osName().find("Mac") != std::string::npos) // if user is running mac
		url = "https://dpoge.github.io/VersionSwitcher/" + which + "-mac.html";
	else if(!Poco::Environment::isWindows() && !Poco::Environment::isUnix()) // not running windows, mac, or linux (prob not even possible, but better safe than sorry)
		wxMessageBox("Platform not supported! Will attempt to use Windows versions.", "Warning", wxICON_EXCLAMATION);
	// do some things that allow streaming to work
	Poco::Net::HTTPSStreamFactory::registerFactory();
	initializeSSL();
	// write versions from github.io page to string
	std::string content;
	try
	{
		auto is = std::shared_ptr<std::istream>{Poco::URIStreamOpener::defaultOpener().open(Poco::URI{url})};
		Poco::StreamCopier::copyToString(*(is.get()), content);
	}
	catch (std::exception& e)
	{
		wxMessageBox("Error occurred getting the version info: " + wxString(e.what()) + ". Make sure if you have a firewall that it is disabled.", "Error", wxICON_ERROR);
	}
	Poco::Net::HTTPSStreamFactory::unregisterFactory();
	// parse to wxArrayString
	std::stringstream ss(content);
	std::string buf;
	wxArrayString result;
	while (ss >> buf)
		result.push_back(buf);

	return result;
}

void downloadFile(const std::string& url, const std::string& fileLocation)
{
	// do some things that allow streaming to work
	Poco::Net::HTTPSStreamFactory::registerFactory();
	initializeSSL();
	try
	{
		// have to do this weird way of threading because wxwidgets is retarded with threads
		std::atomic<bool> done(false);
		std::thread thread([&]() 
		{
			auto is = std::shared_ptr<std::istream>{ Poco::URIStreamOpener::defaultOpener().open(Poco::URI{url}) };
			std::ofstream fileStream(fileLocation, std::ios::out | std::ios::trunc | std::ios::binary);
			Poco::StreamCopier::copyStream(*(is.get()), fileStream);
			fileStream.close();
			done = true;
		});
		thread.detach();
		while (!done)
		{
			Sleep(30);
			wxYield();
		}
	}
	catch (std::exception& e)
	{
		wxMessageBox("Error occurred downloading the file: " + wxString(e.what()), "Error", wxICON_ERROR);
	}
	Poco::Net::HTTPSStreamFactory::unregisterFactory();
}

void unzip(const std::string& file, const std::string& destination)
{
	try
	{
		// the weird way of threading returns!
		std::atomic<bool> done(false);
		std::thread thread([&]() 
		{
			std::ifstream ifs(file, std::ios::binary);
			Poco::Zip::Decompress dec(ifs, destination);
			dec.decompressAllFiles();
			done = true;
		});
		thread.detach();
		while (!done)
		{
			Sleep(30);
			wxYield();
		}
	}
	catch (std::exception& e)
	{
		wxMessageBox("Error occurred unzipping the file: " + wxString(e.what()), "Error", wxICON_ERROR);
	}
}

cMain::cMain() : wxFrame(nullptr, wxID_ANY, "BTD6 Version Switcher", wxDefaultPosition, wxSize(800, 600), wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX) | wxWANTS_CHARS)
{
	const wxArrayString versionInfo = getVersionInfo("versions");
	// set up objects
	openFolderBtn = new wxButton(this, wxID_ANY, "Open BTD6 Folder", wxPoint(70, 95), wxSize(125, 50));
	switchBtn = new wxButton(this, wxID_ANY, "Switch Version", wxPoint(560, 95), wxSize(125, 50));
	discordBtn = new wxButton(this, wxID_ANY, "BTD6 Modding Discord", wxPoint(0, 0), wxSize(160, 50), wxBORDER_NONE);
	versionSelect = new wxComboBox(this, wxID_ANY, versionInfo.at(0), wxPoint(310, 145), wxSize(125, 50), versionInfo, wxCB_READONLY);
	btd6Folder = new wxTextCtrl(this, wxID_ANY, "No folder selected!", wxPoint(195, 65), wxSize(365, 25), wxTE_READONLY);
	console = new wxTextCtrl(this, wxID_ANY, "This is the console. Stuff will be logged here!\nIn the event you encounter a bug, please post it in the #bug-reports channel on the BTD6 Modding Discord or create an issue on the GitHub repository.", wxPoint(0, 220), wxSize(785, 365), wxTE_READONLY | wxTE_MULTILINE | wxTE_NO_VSCROLL);
	// style/bind objects
	this->SetBackgroundColour(wxColour(54, 57, 63));
	openFolderBtn->SetBackgroundColour(wxGREY_BACKGROUND);
	openFolderBtn->SetForegroundColour(wxGREY_FOREGROUND);
	switchBtn->SetBackgroundColour(wxGREY_BACKGROUND);
	switchBtn->SetForegroundColour(wxGREY_FOREGROUND);
	discordBtn->SetBackgroundColour(wxColour(114, 137, 218));
	discordBtn->SetForegroundColour(*wxWHITE);
	btd6Folder->SetBackgroundColour(wxGREY_BACKGROUND);
	btd6Folder->SetForegroundColour(wxGREY_FOREGROUND);
	console->SetBackgroundColour(*wxBLACK);
	console->SetForegroundColour(*wxGREEN);
	openFolderBtn->Bind(wxEVT_BUTTON, &cMain::openBtd6Folder, this);
	switchBtn->Bind(wxEVT_BUTTON, &cMain::switchVersion, this);
	discordBtn->Bind(wxEVT_BUTTON, &cMain::joinDiscord, this);
	this->Bind(wxEVT_CHAR_HOOK, &cMain::hotkeys, this);
}

void cMain::openBtd6Folder(wxCommandEvent& event)
{
	wxString initialDir;
	const std::string dirs[3] = { "C:/Program Files (x86)/Steam/steamapps/common/BloonsTD6", "~/Library/Application Support/Steam/SteamApps/common/BloonsTD6", "~/.steam/steam/SteamApps/common/BloonsTD6" }; // should be default btd6 directory for windows, mac, and linux (in that order)
	for (const auto& dir : dirs)
	{
		if (fs::exists(dir))
			initialDir = dir;
	}
	wxDirDialog* openFolderDialog = new wxDirDialog(NULL, "Choose BTD6 directory", initialDir, wxDD_DEFAULT_STYLE | wxDIALOG_NO_PARENT);
	if (openFolderDialog->ShowModal() == wxID_OK)
	{
		wxString path = openFolderDialog->GetPath();
		path.Replace("\\", "/", true);
		btd6Folder->ChangeValue(path);
		// crack/dumbass protection
		if (path.MakeLower().Find("steamapps/common/bloonstd6") != std::string::npos && fs::exists(path.ToStdString() + "/BloonsTD6_Data"))
		{
			btd6Folder->SetBackgroundColour(*wxGREEN);
			if (!fs::exists(path.ToStdString() + "/versions"))
				fs::create_directory(path.ToStdString() + "/versions");
		}
		else
		{
			btd6Folder->SetBackgroundColour(*wxRED);
		}
	}
	btd6Folder->Refresh();
}

std::string downloads = "downloads";
void cMain::switchVersion(wxCommandEvent& event)
{
	const std::string directory = btd6Folder->GetValue();
	if (directory != "No folder selected!" && btd6Folder->GetBackgroundColour() != *wxRED) // if user selected a valid directory
	{
		const std::string version = versionSelect->GetValue();
		const std::string zipPath = directory + "/versions/" + version + ".zip";
		const wxArrayString versionLinks = getVersionInfo(downloads);
		if (!fs::exists(zipPath))
		{
			console->AppendText("\nDownloading version " + version + "...");
			downloadFile(versionLinks.at(versionSelect->GetSelection()), zipPath);
		}
		else
		{
			console->AppendText("\nZip for " + version + " exists!");
		}
		console->AppendText("\nExtracting...");
		unzip(zipPath, directory);
		console->AppendText("\nDone!\n");
		return;
	}
	wxMessageBox("You haven't selected your BTD6 folder or the selected folder is invalid, please fix that!", "Warning", wxICON_EXCLAMATION);
}

void cMain::joinDiscord(wxCommandEvent& event)
{
	wxLaunchDefaultBrowser("https://discord.gg/MmnUCWV");
}

void cMain::hotkeys(wxKeyEvent& event)
{
	if ((int)event.GetKeyCode() == 344 && wxMessageBox("Refresh versions list?", "F5 was pressed", wxICON_QUESTION | wxYES_NO) == wxYES)
	{
		const wxArrayString versionsOverride = getVersionInfo("versions");
		versionSelect->Set(versionsOverride);
		versionSelect->SetValue(versionsOverride.at(0));
		wxMessageBox("Versions list was refreshed!", "Success");
	}
	else if ((int)event.GetKeyCode() == 127 && wxMessageBox("Clear versions folder?", "Delete was pressed", wxICON_QUESTION | wxYES_NO) == wxYES)
	{
		const std::string path = btd6Folder->GetValue() + "/versions";
		if (fs::exists(path))
		{
			fs::remove_all(path);
			fs::create_directory(path);
			wxMessageBox("Versions folder has been cleared!");
		}
		else
		{
			wxMessageBox("You haven't selected your BTD6 folder, please do that!", "Warning", wxICON_EXCLAMATION);
		}
	}
	else if ((int)event.GetKeyCode() == 77) // different than the others to make toggleable
	{
		int use = wxMessageBox("Use modded versions list?", "M was pressed", wxICON_QUESTION | wxYES_NO);
		wxArrayString versionsOverride = getVersionInfo("versions");
		if (use == wxYES)
		{
			versionsOverride = getVersionInfo("versions-modded");
			downloads = "downloads-modded";
			wxMessageBox("Versions list was refreshed!", "Success");
		}
		versionSelect->Set(versionsOverride);
		versionSelect->SetValue(versionsOverride.at(0));
	}
}