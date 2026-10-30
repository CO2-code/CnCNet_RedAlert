#ifndef REPLAYDLG_H
#define REPLAYDLG_H

#include "defines.h"

/*
**	Maximum number of replay files shown in the open dialog.
*/
#define	MAX_REPLAY_FILES	128


class ListClass;


class ReplayFileEntryClass
{
public:
	char Descr[80];         // replay file name
	unsigned Scenario;      // scenario #
	HousesType House;       // house
	int Num;                // save file number (from the extension)
	unsigned long DateTime; // date/time stamp of file
	bool Valid;             // Is the replay valid?
};


class LoadReplayClass
{
public:
	/*
	** This defines the style of the dialog
	*/
	typedef enum OperationModeEnum
	{
		NONE = 0,
		LOAD,
		SAVE,
		WWDELETE
	} LoadStyleType;

	LoadReplayClass();
	~LoadReplayClass();
	int Process(void);

protected:
	/*
	** Internal routines
	*/
	void Clear_List(ListClass * list);                   // clears the list & Files array
	void Fill_List(ListClass * list);                    // fills the list & Files array
	static int Compare(void const * p1, void const * p2); // for qsort()

	/*
	**	Pointers to ReplayFileEntryClass objects. These objects are allocated on
	**	the fly as files are found and must be freed before the list is cleared.
	**	The array is sorted by date/time so the newest replay is listed first.
	**
	**	A plain fixed size array is used here on purpose: it avoids depending on
	**	out-of-line template instantiations of DynamicVectorClass, which the
	**	Watcom build cannot rely on for a type only used in this module.
	*/
	ReplayFileEntryClass * Files[MAX_REPLAY_FILES];
	int FileCount;
};

#endif
