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
 *                   Programmer :                                                              *
 *                                                                                             *
 *                   Start Date :                                                              *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   LoadReplayClass::LoadReplayClass -- class constructor                                       *
 *   LoadReplayClass::~LoadReplayClass -- class destructor                                       *
 *   LoadReplayClass::Process -- main processing routine                                         *
 *   LoadReplayClass::Clear_List -- clears the list & Files arrays                               *
 *   LoadReplayClass::Fill_List -- fills the list & game # array                                 *
 *   LoadReplayClass::Compare -- for qsort                                                     *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"function.h"
#include	"msgbox.h"
#include	"edit.h"
#include	"drop.h"
#include	"textbtn.h"
#include	"replaydlg.h"
#include	<io.h>		// for _dos_findfirst, _dos_findnext

/***********************************************************************************************
 * LoadReplayClass::LoadReplayClass -- class constructor                                       *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none                                                                                   *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none                                                                                   *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
LoadReplayClass::LoadReplayClass()
{
	Files.Clear();
}


/***********************************************************************************************
 * LoadReplayClass::~LoadReplayClass -- class destructor                                       *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none                                                                                   *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none                                                                                   *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
LoadReplayClass::~LoadReplayClass()
{
	for (int i = 0; i < Files.Count(); i++) {
		delete Files[i];
	}
	Files.Clear();
}

/***********************************************************************************************
 * LoadReplayClass::Process -- main processing routine                                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none                                                                                   *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      false = User cancelled, true = operation completed                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
int LoadReplayClass::Process(void)
{
	/*
	**	Dialog & button dimensions
	*/
	int d_dialog_w = 250 * RESFACTOR;					// dialog width
	int d_dialog_h = 156 * RESFACTOR;					// dialog height
	int d_dialog_x = (((320 * RESFACTOR) - d_dialog_w) / 2);	// centered x-coord
	int d_dialog_y = (((200 * RESFACTOR) - d_dialog_h) / 2);	// centered y-coord
	int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);	// coord of x-center

	int d_txt8_h = 11 * RESFACTOR;						// ht of 8-pt text
	int d_margin = 7 * RESFACTOR;						// margin width/height
	int x_margin = 16 * RESFACTOR;						// margin width/height

	int d_list_w = d_dialog_w - (x_margin * 2);
	int d_list_h = 104 * RESFACTOR;
	int d_list_x = d_dialog_x + x_margin;
	int d_list_y = d_dialog_y + d_margin + d_txt8_h + d_margin;

	int d_edit_w = d_dialog_w - (x_margin * 2);
	int d_edit_h = 13 * RESFACTOR;
	int d_edit_x = d_dialog_x + x_margin;
	int d_edit_y = d_list_y + d_list_h - (30 * RESFACTOR) + d_margin + d_txt8_h;

#if (GERMAN | FRENCH)
	int d_button_w = 50 * RESFACTOR;
#else
	int d_button_w = 40 * RESFACTOR;
#endif
	int d_button_h = 13 * RESFACTOR;
	int d_button_x = d_dialog_cx - d_button_w - d_margin;
	int d_button_y = d_dialog_y + d_dialog_h - d_button_h - d_margin;

#if defined(GERMAN) || defined(FRENCH)
	int d_cancel_w = 60 * RESFACTOR;	//BG:40
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
		BUTTON_LIST,
		BUTTON_EDIT,
	};

	/*
	**	Redraw values: in order from "top" to "bottom" layer of the dialog
	*/
	typedef enum {
		REDRAW_NONE = 0,
		REDRAW_BUTTONS,
		REDRAW_BACKGROUND,
		REDRAW_ALL = REDRAW_BACKGROUND
	} RedrawType;

	/*
	**	Dialog variables
	*/
	bool cancel = false;		// true = user cancels
	int list_ht = d_list_h;	// adjusted list box height

	/*
	**	Other Variables
	*/
	int game_idx = 0;			// index of game to load
	int game_num = 0;			// file number of game to load
	char game_descr[DESCRIP_MAX] = {0};	// save-game description

	/*
	**	Buttons
	*/
	ControlClass * commands = NULL;	// the button list

	int btn_txt = TXT_LOAD_BUTTON;
	int btn_id = BUTTON_LOAD;
	int caption = TXT_LOAD_MISSION;

	TextButtonClass button (btn_id, btn_txt, TPF_BUTTON, d_button_x, d_button_y, d_button_w);
	TextButtonClass cancelbtn (BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON, d_cancel_x, d_cancel_y, d_cancel_w);

	ListClass listbtn (BUTTON_LIST, d_list_x, d_list_y, d_list_w, list_ht,
		TPF_6PT_GRAD | TPF_NOSHADOW,
		MFCD::Retrieve("BTN-UP.SHP"),
		MFCD::Retrieve("BTN-DN.SHP"));

	EditClass editbtn (BUTTON_EDIT, game_descr, sizeof(game_descr)-4, TPF_6PT_GRAD|TPF_NOSHADOW, d_edit_x, d_edit_y, d_edit_w, -1, EditClass::ALPHANUMERIC);

	/*
	**	Initialize.
	*/
	Set_Logic_Page(SeenBuff);

	Fill_List(&listbtn);

	/*
	**	Do nothing if list is empty.
	*/
	if (listbtn.Count()==0) {
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
	bool firsttime = true;
	bool display = true;
	bool process = true;
	while (process) {

		/*
		**	Invoke game callback.
		*/
		if (Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH) {
			Call_Back();
		} else {
			if (Main_Loop()) {
				process = false;
				cancel = true;
			}
		}

		/*
		**	If we have just received input focus again after running in the background then
		**	we need to redraw.
		*/
#ifdef WIN32
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
			/*
			**	Display the dialog box.
			*/
			if (display) {
				Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
				Draw_Caption(caption, d_dialog_x, d_dialog_y, d_dialog_w);
			}

			/*
			**	Redraw the buttons.
			*/
			if (display) {
				commands->Flag_List_To_Redraw();
			}
			Show_Mouse();
			display = false;
		}

		/*
		**	Get user input.
		*/
		KeyNumType input = commands->Input();

		/*
		**	The first time through the processing loop, set the edit
		**	gadget to have the focus if this is the save dialog. The
		**	focus must be set here since the gadget list has changed
		**	and this change will cause any previous focus setting to be
		**	cleared by the input processing routine.
		*/
		firsttime = false;
		// editbtn.Set_Focus();
		editbtn.Flag_To_Redraw();

		/*
		**	If the <RETURN> key was pressed, then default to the appropriate
		**	action button according to the style of this dialog box.
		*/
		if (input == KN_RETURN || input == (BUTTON_EDIT | KN_BUTTON)) {
			ToggleClass* toggle = NULL;
			input = (KeyNumType)(BUTTON_LOAD | KN_BUTTON);
			cancelbtn.Turn_Off();
			toggle = (ToggleClass*)commands->Extract_Gadget(BUTTON_LOAD);
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
		**	Load: if load fails, present a message, and stay in the dialog
		**	to allow the user to try another game
		*/
		case (BUTTON_LOAD | KN_BUTTON):
			game_idx = listbtn.Current_Index();
			game_num = Files[game_idx]->Num;

			if (Files[game_idx]->Valid) {
				Session.RecordFile.Set_Name(listbtn.Current_Item());
				Session.Play = true;
				Session.Record = false;
				process = false;
			} else {
				WWMessageBox().Process(TXT_OBSOLETE_SAVEGAME);
			}
			break;

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

	if (cancel)
		return (false);

	return (true);
}

/***********************************************************************************************
 * LoadReplayClass::Clear_List -- clears the list & Files arrays                               *
 *                                                                                             *
 * This step is essential, because it frees all the strings allocated for list items.          *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none                                                                                   *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none                                                                                   *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
void LoadReplayClass::Clear_List(ListClass* list)
{
	/*
	**	For every item in the list, free its buffer & remove it from the list.
	*/
	int j = list->Count();
	for (int i = 0; i < j; i++) {
		list->Remove_Item(list->Get_Item(0));
	}

	/*
	**	Clear the array of game numbers
	*/
	for (int k = 0; k < Files.Count(); k++) {
		delete Files[k];
	}
	Files.Clear();
}


/***********************************************************************************************
 * LoadReplayClass::Fill_List -- fills the list & GameNum arrays                               *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none                                                                                   *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none                                                                                   *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
void LoadReplayClass::Fill_List(ListClass* list)
{
	ReplayFileEntryClass* fdata;	// for adding entries to 'Files'
	struct find_t ff;		// for _dos_findfirst
	int id = 0;

	/*
	**	Make sure the list is empty
	*/
	Clear_List(list);

	/*
	**	Find all replay files
	*/
	int rc = _dos_findfirst("*.REPLAY*", _A_NORMAL, &ff);

	while (rc) {

		/*
		**	Extract the game ID from the filename
		*/
		id = id++;

		fdata = new ReplayFileEntryClass;

		fdata->Descr[0] = '\0';

		strncat(fdata->Descr, ff.name, (sizeof(fdata->Descr) - strlen(fdata->Descr)) - 1);
		fdata->Valid = true;
		fdata->Scenario = 0;
		fdata->House = (HousesType)0;
		fdata->Num = id++;
		fdata->DateTime = 0;
		Files.Add(fdata);

		/*
		**	Find the next file
		*/
		rc = _dos_findnext(&ff);
	}

	/*
	**	Now sort the list in order of Date/Time (newest first, oldest last)
	*/
	qsort((void*)(&Files[0]), Files.Count(), sizeof(class ReplayFileEntryClass*), LoadReplayClass::Compare);

	/*
	**	Now add every file's name to the list box
	*/
	for (int i = 0; i < Files.Count(); i++) {
		list->Add_Item(Files[i]->Descr);
	}
}


/***********************************************************************************************
 * LoadReplayClass::Compare -- for qsort                                                       *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      p1,p2      ptrs to elements to compare                                                 *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      0 = same, -1 = (*p1) goes BEFORE (*p2), 1 = (*p1) goes AFTER (*p2)                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   Created.                                                                                  *
 *=============================================================================================*/
int LoadReplayClass::Compare(const void* p1, const void* p2)
{
	class ReplayFileEntryClass *fe1, *fe2;

	fe1 = *((class ReplayFileEntryClass**)p1);
	fe2 = *((class ReplayFileEntryClass**)p2);

	if (fe1->DateTime > fe2->DateTime)
		return (-1);
	if (fe1->DateTime < fe2->DateTime)
		return (1);
	return (0);
}
