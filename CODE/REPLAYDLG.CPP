/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : REPLAYDLG.CPP                                                *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   LoadReplayClass::LoadReplayClass -- class constructor                                     *
 *   LoadReplayClass::~LoadReplayClass -- class destructor                                     *
 *   LoadReplayClass::Process -- main processing routine                                       *
 *   LoadReplayClass::Clear_List -- clears the list & Files array                              *
 *   LoadReplayClass::Fill_List -- fills the list & Files array                                *
 *   LoadReplayClass::Compare -- for qsort                                                     *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"function.h"
#include	"msgbox.h"
#include	"textbtn.h"
#include	"REPLAYDLG.H"
#include	<io.h>		// for _dos_findfirst / _dos_findnext


/***********************************************************************************************
 * LoadReplayClass::LoadReplayClass -- class constructor                                       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
LoadReplayClass::LoadReplayClass()
{
	FileCount = 0;
	for (int i = 0; i < MAX_REPLAY_FILES; i++) {
		Files[i] = NULL;
	}
}


/***********************************************************************************************
 * LoadReplayClass::~LoadReplayClass -- class destructor                                       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
LoadReplayClass::~LoadReplayClass()
{
	for (int entry = 0; entry < FileCount; entry++) {
		delete Files[entry];
		Files[entry] = NULL;
	}
	FileCount = 0;
}

/***********************************************************************************************
 * LoadReplayClass::Process -- main processing routine                                         *
 *                                                                                             *
 * Lists every *.REPLAY* file in the game directory (newest first) and lets the player pick    *
 * one. On success the selected file is handed to Session.RecordFile and Session.Play is set   *
 * so the caller opens it and starts playback.                                                *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  false = User cancelled, true = a replay file was selected                          *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
int LoadReplayClass::Process(void)
{
	/*
	**	Dialog & button dimensions
	*/
	int d_dialog_w = 250 * RESFACTOR;									// dialog width
	int d_dialog_h = 156 * RESFACTOR;									// dialog height
	int d_dialog_x = (((320 * RESFACTOR) - d_dialog_w) / 2);				// centered x-coord
	int d_dialog_y = (((200 * RESFACTOR) - d_dialog_h) / 2);				// centered y-coord
	int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);		// coord of x-center

	int d_txt8_h = 11 * RESFACTOR;												// ht of 8-pt text
	int d_margin = 7 * RESFACTOR;												// margin width/height
	int x_margin = 16 * RESFACTOR;												// list side margin

	int d_list_w = d_dialog_w - (x_margin * 2);
	int d_list_h = 104 * RESFACTOR;
	int d_list_x = d_dialog_x + x_margin;
	int d_list_y = d_dialog_y + d_margin + d_txt8_h + d_margin;

#if (GERMAN | FRENCH)
	int d_button_w = 50 * RESFACTOR;
#else
	int d_button_w = 40 * RESFACTOR;
#endif
	int d_button_h = 13 * RESFACTOR;
	int d_button_x = d_dialog_cx - d_button_w - d_margin;
	int d_button_y = d_dialog_y + d_dialog_h - d_button_h - d_margin;

#if defined(GERMAN) || defined(FRENCH)
	int d_cancel_w = 60 * RESFACTOR;
#else
	int d_cancel_w = 40 * RESFACTOR;
#endif
	int d_cancel_h = 13 * RESFACTOR;
	int d_cancel_x = d_dialog_cx + d_margin;
	int d_cancel_y = d_dialog_y + d_dialog_h - d_cancel_h - d_margin;

	/*
	**	Button enumerations
	*/
	enum {
		BUTTON_LOAD = 100,
		BUTTON_CANCEL,
		BUTTON_LIST
	};

	/*
	**	Dialog variables
	*/
	bool cancel = false;						// true = user cancels

	/*
	**	Buttons
	*/
	ControlClass * commands = NULL;			// the button list

	TextButtonClass button(BUTTON_LOAD, TXT_LOAD_BUTTON, TPF_BUTTON, d_button_x, d_button_y, d_button_w);
	TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON, d_cancel_x, d_cancel_y, d_cancel_w);

	ListClass listbtn(BUTTON_LIST, d_list_x, d_list_y, d_list_w, d_list_h,
		TPF_6PT_GRAD | TPF_NOSHADOW,
		MFCD::Retrieve("BTN-UP.SHP"),
		MFCD::Retrieve("BTN-DN.SHP"));

	/*
	**	Initialize.
	*/
	Set_Logic_Page(SeenBuff);

	Fill_List(&listbtn);

	/*
	**	Do nothing if list is empty.
	*/
	if (listbtn.Count() == 0) {
		Clear_List(&listbtn);
		WWMessageBox().Process(TXT_NO_SAVES);
		return(false);
	}

	/*
	**	Create the button list.
	*/
	commands = &button;
	cancelbtn.Add_Tail(*commands);
	listbtn.Add_Tail(*commands);

	/*
	**	Main Processing Loop.
	*/
	Keyboard->Clear();
	bool display = true;
	bool process = true;
	while (process) {

		/*
		**	Invoke game callback.
		*/
		Call_Back();

#ifdef WIN32
		/*
		** If we have just received input focus again after running in the background then
		** we need to redraw.
		*/
		if (AllSurfaces.SurfacesRestored) {
			AllSurfaces.SurfacesRestored=FALSE;
			display = true;
		}
#endif

		/*
		**	Refresh display if needed.
		*/
		if (display) {
			Hide_Mouse();
			Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
			Draw_Caption("Load Replay", d_dialog_x, d_dialog_y, d_dialog_w);
			commands->Flag_List_To_Redraw();
			Show_Mouse();
			display = false;
		}

		/*
		**	Get user input.
		*/
		KeyNumType input = commands->Input();

		/*
		**	If the <RETURN> key was pressed, default to the LOAD button.
		*/
		if (input == KN_RETURN) {
			ToggleClass * toggle = NULL;
			input = (KeyNumType)(BUTTON_LOAD | KN_BUTTON);
			cancelbtn.Turn_Off();
			toggle = (ToggleClass *)commands->Extract_Gadget(BUTTON_LOAD);
			if (toggle != NULL) {
				toggle->IsOn = true;
				toggle->IsPressed = true;
			}
			Hide_Mouse();
			commands->Draw_All(true);
			Show_Mouse();
		}

		/*
		**	Process input.
		*/
		switch (input) {

			/*
			**	Load: hand the selected replay to the caller.
			*/
			case (BUTTON_LOAD | KN_BUTTON):
			{
				int game_idx = listbtn.Current_Index();
				if (game_idx >= 0 && game_idx < FileCount && Files[game_idx]->Valid) {
					Session.RecordFile.Set_Name(Files[game_idx]->FilePath);
					Session.Play = true;
					Session.Record = false;
					process = false;
				} else {
					WWMessageBox().Process(TXT_OBSOLETE_SAVEGAME);
				}
				break;
			}

			/*
			**	ESC/Cancel: break
			*/
			case (KN_ESC):
			case (BUTTON_CANCEL | KN_BUTTON):
				cancel = true;
				process = false;
				break;

			default:
				break;
		}
	}

	Clear_List(&listbtn);

	if (cancel) {
		return(false);
	}
	return(true);
}

/***********************************************************************************************
 * LoadReplayClass::Clear_List -- clears the list box & Files array                           *
 *                                                                                             *
 * This step is essential, because it frees all the strings allocated for list items.          *
 *                                                                                             *
 * INPUT:   list  -- the list box to clear                                                     *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
void LoadReplayClass::Clear_List(ListClass * list)
{
	/*
	**	For every item in the list, free its buffer & remove it from the list.
	*/
	int j = list->Count();
	for (int i = 0; i < j; i++) {
		list->Remove_Item(list->Get_Item(0));
	}

	/*
	**	Free the replay entries.
	*/
	for (int entry = 0; entry < FileCount; entry++) {
		delete Files[entry];
		Files[entry] = NULL;
	}
	FileCount = 0;
}


/***********************************************************************************************
 * LoadReplayClass::Fill_List -- fills the list box & Files array                             *
 *                                                                                             *
 * Scans the current directory for *.REPLAY* files. Each file is added as its own entry and   *
 * the entries are sorted newest first so the most recent match is at the top of the list.    *
 *                                                                                             *
 * INPUT:   list  -- the list box to fill                                                      *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
void LoadReplayClass::Fill_List(ListClass * list)
{
	ReplayFileEntryClass * fdata;
	struct find_t ff;
	const char * replay_dirs[2] = {"", "replays\\"};
	char search[128];
	int id = 0;

	Clear_List(list);

	for (int dir = 0; dir < 2 && FileCount < MAX_REPLAY_FILES; dir++) {
		search[0] = '\0';
		strncat(search, replay_dirs[dir], sizeof(search) - 1);
		strncat(search, "*.REPLAY*", sizeof(search) - strlen(search) - 1);
		int rc = _dos_findfirst(search, _A_NORMAL, &ff);

		while (!rc && FileCount < MAX_REPLAY_FILES) {
			fdata = new ReplayFileEntryClass;
			fdata->Descr[0] = '\0';
			strncat(fdata->Descr, ff.name, sizeof(fdata->Descr) - 1);
			fdata->FilePath[0] = '\0';
			strncat(fdata->FilePath, replay_dirs[dir], sizeof(fdata->FilePath) - 1);
			strncat(fdata->FilePath, ff.name, sizeof(fdata->FilePath) - strlen(fdata->FilePath) - 1);
			fdata->Valid = true;
			fdata->Scenario = 0;
			fdata->House = (HousesType)0;
			fdata->Num = id++;
			fdata->DateTime = (((unsigned long)ff.wr_date) << 16) | (unsigned long)ff.wr_time;
			Files[FileCount++] = fdata;
			rc = _dos_findnext(&ff);
		}
	}

	if (FileCount > 1) {
		qsort((void *)(&Files[0]), FileCount, sizeof(ReplayFileEntryClass *), LoadReplayClass::Compare);
	}

	for (int i = 0; i < FileCount; i++) {
		list->Add_Item(Files[i]->Descr);
	}
}

/***********************************************************************************************
 * LoadReplayClass::Compare -- for qsort                                                      *
 *                                                                                             *
 * INPUT:   p1,p2      ptrs to elements to compare                                             *
 *                                                                                             *
 * OUTPUT:  0 = same, -1 = (*p1) goes BEFORE (*p2), 1 = (*p1) goes AFTER (*p2)                 *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/29/2026 : Created to back the replay menu.                                             *
 *=============================================================================================*/
int LoadReplayClass::Compare(void const * p1, void const * p2)
{
	ReplayFileEntryClass * fe1;
	ReplayFileEntryClass * fe2;

	fe1 = *((ReplayFileEntryClass **)p1);
	fe2 = *((ReplayFileEntryClass **)p2);

	if (fe1->DateTime > fe2->DateTime) {
		return(-1);
	}
	if (fe1->DateTime < fe2->DateTime) {
		return(1);
	}
	return(0);
}
