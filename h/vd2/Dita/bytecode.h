#ifndef f_VD2_DITA_BYTECODE_H
#define f_VD2_DITA_BYTECODE_H

namespace nsVDDitaBytecode {
	enum {
		kBC_End,
		kBC_Zero,				// push integer 0 onto stack
		kBC_One,				// push integer 1 onto stack
		kBC_Int8,				// push signed 8-bit integer onto stack
		kBC_Int32,				// push full 32-bit integer onto stack
		kBC_String,				// push string from string table onto stack
		kBC_StringShort,		// push string from string table -1 onto stack

		kBC_InvokeTemplate,

		kBC_CreateLabel	= 0x80,	// create label(id, string, maxwidth)
		kBC_CreateEdit,			// create edit(id, label, maxlen)
		kBC_CreateEditInt,		// create editInt(id, initial, min, max)
		kBC_CreateButton,		// create button(id, label)
		kBC_CreateCheckBox,		// create checkbox(id, label)
		kBC_CreateListBox,		// create listbox(id, minrows)
		kBC_CreateComboBox,		// create combobox(id, minrows)
		kBC_CreateListView,		// create listview(id, minrows)
		kBC_CreateTrackbar,		// create trackbar(id, minv, maxv)
		kBC_CreateFileControl,	// create filecontrol(id, maxlen)
		kBC_CreateOptionSet,	// create optionset(id)
		kBC_AddOption,			// create option(label)
		kBC_EndOptionSet,		//
		kBC_CreateHorizSet,		// create horizset(id)
		kBC_CreateVertSet,		// create vertset(id)
		kBC_CreateGroupSet,		// create groupset(id, label)
		kBC_EndSet,				// end set
		kBC_CreateGrid,			// create grid(id, cols, rows, xpad, ypad, affinity)
		kBC_EndGrid,			// end grid
		kBC_SetGridDirection,	// set grid direction(dir)
		kBC_SpanNext,			// spanNext(w,h)
		kBC_SkipGrid,			// skipGrid(x,y)
		kBC_SetGridPos,			// setGridPos(x,y)
		kBC_SetColumnAffinity,	// setColumnAffinity(x, aff)
		kBC_SetRowAffinity,		// setRowAffinity(y, aff)
		kBC_SetDialogInfo,		// setDialogInfo(minw, minh, aspect, title)
		kBC_SetAlignment,		// setAlignment(alignw, alignh)
		kBC_SetMinSize,			// setMinSize(minw, minh)
		kBC_SetMaxSize,			// setMaxSize(maxw, maxh)
		kBC_Link,				// link(dst, src, linktype)
		kBC_AddColumn			// addColumn(name, width)
	};
};

#endif
