#pragma once
#include "wx/wx.h"

class cMain : public wxFrame
{
public:
	cMain();
	wxButton* openFolderBtn;
	wxButton* switchBtn;
	wxButton* discordBtn;
	wxComboBox* versionSelect;
	wxTextCtrl* btd6Folder;
	wxTextCtrl* console;
	void openBtd6Folder(wxCommandEvent& event);
	void switchVersion(wxCommandEvent& event);
	void joinDiscord(wxCommandEvent& event);
	void hotkeys(wxKeyEvent& event);
};
