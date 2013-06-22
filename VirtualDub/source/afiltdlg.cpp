//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>

#include <vd2/system/thread.h>
#include <vd2/system/refcount.h>
#include <vd2/system/error.h>
#include <vd2/system/registry.h>
#include "resource.h"
#include "gui.h"
#include "AudioFilterSystem.h"
#include "FilterGraph.h"
#include "filter.h"

extern const char g_szError[];
extern const char g_szWarning[];

class AudioSource;

extern AudioSource *inputAudio;

///////////////////////////////////////////////////////////////////////////

namespace {
	class AudioFilterData : public vdrefcounted<IVDRefCount> {
	public:
		VDStringW			mName;
		VDFilterConfig		mConfigBlock;
		bool				mbHasConfigDialog;
	};
}

///////////////////////////////////////////////////////////////////////////

// yuk.

namespace {
	HWND g_hdlgNestedModeless;
	HHOOK g_hhkNestedModeless;

	static LRESULT CALLBACK VDNestedModelessDialogHookW32(int code, WPARAM wParam, LPARAM lParam) {
		static bool s_bRecursive = false;

		if (!s_bRecursive && code == MSGF_DIALOGBOX) {
			s_bRecursive = true;
			BOOL bAbsorb = IsDialogMessage(g_hdlgNestedModeless, (MSG *)lParam);
			s_bRecursive = false;

			return bAbsorb;
		}

		return CallNextHookEx(g_hhkNestedModeless, code, wParam, lParam);
	}
}

///////////////////////////////////////////////////////////////////////////

class IVDDialogAddAudioFilterCallbackW32 {
public:
	virtual void FilterSelected() = 0;
	virtual void FilterDialogClosed() = 0;
};

class VDDialogAddAudioFilterW32 : public VDDialogBaseW32 {
public:
	VDDialogAddAudioFilterW32(IVDDialogAddAudioFilterCallbackW32 *pParent) : VDDialogBaseW32(IDD_AF_LIST), mpSelectedFilter(NULL), mpParent(pParent) {
		VDEnumerateAudioFilters(mAudioFilters);
	}

	const VDAudioFilterDefinition *GetFilter() const { return mpSelectedFilter; }

protected:
	BOOL DlgProc(UINT message, UINT wParam, LONG lParam);

	void InitDialog();
	void DestroyModeless();
	void UpdateDescription();

	std::list<VDAudioFilterBlurb> mAudioFilters;
	const VDAudioFilterDefinition *mpSelectedFilter;

	IVDDialogAddAudioFilterCallbackW32 *mpParent;
};

BOOL VDDialogAddAudioFilterW32::DlgProc(UINT msg, UINT wParam, LONG lParam) {
	switch(msg) {
        case WM_INITDIALOG:
			InitDialog();
			SetFocus(GetDlgItem(mhdlg, IDC_FILTER_LIST));
            return FALSE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ADD:
				if (mpSelectedFilter)
					mpParent->FilterSelected();
				return TRUE;
			case IDCANCEL:
				DestroyModeless();
				return TRUE;
			case IDC_FILTER_LIST:
				switch(HIWORD(wParam)) {
				case LBN_SELCHANGE:
					UpdateDescription();
					break;
				case LBN_DBLCLK:
					if (mpSelectedFilter)
						mpParent->FilterSelected();
					break;
				}
			}
			break;
	}
	return FALSE;
}

void VDDialogAddAudioFilterW32::InitDialog() {

	// reposition window

	RECT r, r2;
	GetWindowRect(GetParent(mhdlg), &r);
	GetWindowRect(mhdlg, &r2);
	r2.right -= r2.left;
	r2.bottom -= r2.top;
	r2.left = r.right;
	r2.top = r.top;
	r2.right += r2.left;
	r2.bottom += r2.top;

	if (r2.right <= GetSystemMetrics(SM_CXSCREEN) && r2.bottom <= GetSystemMetrics(SM_CYSCREEN))
		SetWindowPos(mhdlg, NULL, r2.left, r2.top, r2.right-r2.left, r2.bottom-r2.top, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);

	// fill filter list

	HWND hwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	INT tabs[]={ 175 };
	SendMessage(hwndList, LB_SETTABSTOPS, 1, (LPARAM)tabs);

	for(std::list<VDAudioFilterBlurb>::const_iterator it(mAudioFilters.begin()), itEnd(mAudioFilters.end()); it!=itEnd; ++it) {
		const VDAudioFilterBlurb& b = *it;

		if (b.name[0] != '*') {
			char buf[1024];

			sprintf(buf, "%ls\t%ls", b.name.c_str(), b.author.c_str());
			int idx = SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)buf);

			if (idx != LB_ERR)
				SendMessage(hwndList, LB_SETITEMDATA, idx, (LPARAM)&b);
		}
	}

	g_hhkNestedModeless = SetWindowsHookEx(WH_MSGFILTER, VDNestedModelessDialogHookW32, NULL, GetCurrentThreadId());
	g_hdlgNestedModeless = mhdlg;
}

void VDDialogAddAudioFilterW32::DestroyModeless() {
	if (g_hhkNestedModeless) {
		UnhookWindowsHookEx(g_hhkNestedModeless);
		g_hhkNestedModeless = 0;
	}
	mpParent->FilterDialogClosed();
	VDDialogBaseW32::DestroyModeless();
}

void VDDialogAddAudioFilterW32::UpdateDescription() {
	HWND hwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);

	mpSelectedFilter = NULL;
	if (sel >= 0)
		if (const VDAudioFilterBlurb *pb = (const VDAudioFilterBlurb *)SendMessage(hwndList, LB_GETITEMDATA, sel, 0)) {
			SetDlgItemText(mhdlg, IDC_FILTER_INFO, VDTextWToA(pb->description).c_str());
			mpSelectedFilter = pb->pDef;
		}
}

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterPreviewThread : public VDThread {
public:
	VDAudioFilterPreviewThread();
	~VDAudioFilterPreviewThread();

	bool Start(const VDAudioFilterGraph& graph);
	void Stop();

protected:
	void ThreadRun();

	VDAudioFilterSystem			mFilterSys;
	VDAtomicInt					mRequestExit;
};

VDAudioFilterPreviewThread::VDAudioFilterPreviewThread()
	: mRequestExit(0)
{
}

VDAudioFilterPreviewThread::~VDAudioFilterPreviewThread() {
}

bool VDAudioFilterPreviewThread::Start(const VDAudioFilterGraph& graph) {
	Stop();

	// copy the graph and replace Output nodes with Preview nodes

	VDAudioFilterGraph graph2(graph);

	VDAudioFilterGraph::FilterList::iterator it(graph2.mFilters.begin()), itEnd(graph2.mFilters.end());

	for(; it!=itEnd; ++it) {
		VDAudioFilterGraph::FilterEntry& filt = *it;

		if (filt.mFilterName == L"output")
			filt.mFilterName = L"*playback";
	}

	{
		std::vector<IVDAudioFilterInstance *> filterPtrs;
		mFilterSys.LoadFromGraph(graph2, filterPtrs);
	}
	mFilterSys.Start();

	mRequestExit=false;
	return ThreadStart();
}

void VDAudioFilterPreviewThread::Stop() {
	mRequestExit = true;
	ThreadWait();
}

void VDAudioFilterPreviewThread::ThreadRun() {
	while(!mRequestExit) {
		if (!mFilterSys.Run()) {
			VDDEBUG("AudioFilterPreview: Audio filter graph has halted.\n");
			break;
		}
	}
	mFilterSys.Stop();
}

///////////////////////////////////////////////////////////////////////////

class VDDialogAudioFiltersW32 : public VDDialogBaseW32, public IVDDialogAddAudioFilterCallbackW32, public IVDFilterGraphControlCallback {
public:
	inline VDDialogAudioFiltersW32(VDAudioFilterGraph& afg) : VDDialogBaseW32(IDD_AF_SETUP), mAddDialog(this), mGraph(afg) {}

	void Activate(VDGUIHandle hParent) {
		ActivateDialog(hParent);
	}

protected:
	BOOL DlgProc(UINT msg, UINT wParam, LONG lParam);
	void InitDialog();
	void SaveDialogSettings();
	void FilterSelected() {
		if (const VDAudioFilterDefinition *pDef = mAddDialog.GetFilter()) {
			vdrefptr<AudioFilterData> afd(new_nothrow AudioFilterData);

			if (afd) {
				afd->mName				= pDef->pszName;
				afd->mbHasConfigDialog	= 0 != (pDef->mFlags & kVFAF_HasConfig);

				mpGraphControl->AddFilter(pDef->pszName, pDef->mInputPins, pDef->mOutputPins, false, afd);
			}
		}
	}

	void FilterDialogClosed() {
		EnableWindow(GetDlgItem(mhdlg, IDC_ADD), TRUE);
	}

	void SelectionChanged(IVDRefCount *pInstance);

	bool Configure(VDGUIHandle hParent, IVDRefCount *pInstance) {
		try {
			AudioFilterData *pd = static_cast<AudioFilterData *>(pInstance);
			VDAudioFilterSystem afs;

			const VDAudioFilterDefinition *pDef = VDLookupAudioFilterByName(pd->mName.c_str());

			if (!pDef)
				throw MyError("Audio filter \"%s\" is not loaded.", VDTextWToA(pd->mName).c_str());

			IVDAudioFilterInstance *pInst = afs.Create(pDef);

			if (pInst) {
				pInst->DeserializeConfig(pd->mConfigBlock);
				pInst->Configure(hParent);
				pInst->SerializeConfig(pd->mConfigBlock);
			}

		} catch(const MyError& e) {
			e.post((HWND)hParent, g_szError);
		}
		return true;
	}

	void Preview();
	void LoadGraph(IVDFilterGraphControl *pSrcGraph, const VDAudioFilterGraph& graph);
	void SaveGraph(VDAudioFilterGraph& graph, IVDFilterGraphControl *pSrcGraph);

	IVDFilterGraphControl *mpGraphControl;

	VDAudioFilterPreviewThread	mPreview;
	VDDialogAddAudioFilterW32	mAddDialog;
	VDAudioFilterGraph&			mGraph;
};

BOOL VDDialogAudioFiltersW32::DlgProc(UINT msg, UINT wParam, LONG lParam) {
	switch(msg) {

        case WM_INITDIALOG:
			InitDialog();
            return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ADD:
				if (!mAddDialog.IsActive()) {
					if (mAddDialog.CreateModeless((VDGUIHandle)mhdlg))
						EnableWindow((HWND)lParam, FALSE);
				}

				return TRUE;
			case IDC_DELETE:
				mpGraphControl->DeleteSelection();
				return TRUE;
			case IDC_CONFIGURE:
				mpGraphControl->ConfigureSelection();
				return TRUE;
			case IDC_CLEAR:
				if (MessageBox(mhdlg, "Clear filter graph?", g_szWarning, MB_ICONEXCLAMATION|MB_OKCANCEL)==IDOK){
					mpGraphControl->SetFilterGraph(std::vector<VDFilterGraphNode>(), std::vector<VDFilterGraphConnection>());
				}
				return TRUE;
			case IDC_TEST:
				Preview();
				return TRUE;
			case IDC_ARRANGE:
				mpGraphControl->Arrange();
				return TRUE;

			case IDC_AUTOARRANGE:
				if (HIWORD(wParam) == BN_CLICKED)
					mpGraphControl->EnableAutoArrange(BST_CHECKED == (3&SendMessage((HWND)lParam, BM_GETSTATE, 0, 0)));
				return TRUE;

			case IDC_AUTOCONNECT:
				if (HIWORD(wParam) == BN_CLICKED)
					mpGraphControl->EnableAutoConnect(BST_CHECKED == (3&SendMessage((HWND)lParam, BM_GETSTATE, 0, 0)));
				return TRUE;

			case IDOK:
				mPreview.Stop();
				SaveGraph(mGraph, mpGraphControl);
				SaveDialogSettings();
				End(TRUE);
				return TRUE;
			case IDCANCEL:
				mPreview.Stop();
				End(FALSE);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

void VDDialogAudioFiltersW32::Preview() {
	try {
		VDAudioFilterGraph graph;
		SaveGraph(graph, mpGraphControl);
		mPreview.Start(graph);
	} catch(const MyError& e) {
		e.post(mhdlg, g_szError);
	}
}

void VDDialogAudioFiltersW32::LoadGraph(IVDFilterGraphControl *pSrcGraph, const VDAudioFilterGraph& graph) {
	std::vector<VDFilterGraphNode> nodes;
	std::vector<VDFilterGraphConnection> connections;

	// convert filters

	{
		for(std::list<VDAudioFilterGraph::FilterEntry>::const_iterator it(graph.mFilters.begin()), itEnd(graph.mFilters.end()); it!=itEnd; ++it) {
			const VDAudioFilterGraph::FilterEntry& f = *it;

			nodes.push_back(VDFilterGraphNode());
			VDFilterGraphNode& node = nodes.back();

			node.name		= f.mFilterName.c_str();
			node.inputs		= f.mInputPins;
			node.outputs	= f.mOutputPins;

			AudioFilterData *pfd = new_nothrow AudioFilterData;

			if (pfd) {
				pfd->mName			= f.mFilterName;
				pfd->mConfigBlock	= f.mConfig;
			}

			node.pInstance	= pfd;
		}
	}

	// convert connections

	{
		for(std::vector<VDAudioFilterGraph::FilterConnection>::const_iterator it(graph.mConnections.begin()), itEnd(graph.mConnections.end()); it!=itEnd; ++it) {
			const VDAudioFilterGraph::FilterConnection& conn = *it;

			VDFilterGraphConnection c;

			c.srcfilt = conn.filt;
			c.srcpin = conn.pin;

			connections.push_back(c);
		}
	}
	pSrcGraph->SetFilterGraph(nodes, connections);
}

void VDDialogAudioFiltersW32::SaveGraph(VDAudioFilterGraph& graph, IVDFilterGraphControl *pSrcGraph) {
	std::vector<VDFilterGraphNode> nodes;
	std::vector<VDFilterGraphConnection> connections;

	pSrcGraph->GetFilterGraph(nodes, connections);

	graph.mConnections.clear();
	graph.mFilters.clear();

	const int nFilters = nodes.size();
	int i;

	std::vector<IVDAudioFilterInstance *> filters(nFilters);

	for(i=0; i<nFilters; ++i) {
		const VDFilterGraphNode& node = nodes[i];
		AudioFilterData *pd = static_cast<AudioFilterData *>(node.pInstance);

		graph.mFilters.push_back(VDAudioFilterGraph::FilterEntry());
		VDAudioFilterGraph::FilterEntry& e = graph.mFilters.back();

		e.mFilterName	= node.name;
		e.mInputPins	= node.inputs;
		e.mOutputPins	= node.outputs;
		e.mConfig		= pd->mConfigBlock;
	}

	const VDFilterGraphConnection *pConn = &connections.front();

	for(i=0; i<nFilters; ++i) {
		const VDFilterGraphNode& node = nodes[i];
		IVDAudioFilterInstance *pDst = filters[i];

		for(int j=0; j<node.inputs; ++j) {
			VDAudioFilterGraph::FilterConnection c;

			c.filt	= pConn->srcfilt;
			c.pin	= pConn->srcpin;

			graph.mConnections.push_back(c);
			
			++pConn;
		}
	}
}

void VDDialogAudioFiltersW32::SelectionChanged(IVDRefCount *pInstance) {
	AudioFilterData *pd = static_cast<AudioFilterData *>(pInstance);

	EnableWindow(GetDlgItem(mhdlg, IDC_DELETE), pd != 0);
	EnableWindow(GetDlgItem(mhdlg, IDC_CONFIGURE), pd && pd->mbHasConfigDialog);
}

void VDDialogAudioFiltersW32::InitDialog() {
	mpGraphControl = VDGetIFilterGraphControl(GetDlgItem(mhdlg, IDC_GRAPH));
	mpGraphControl->SetCallback(this);

	LoadGraph(mpGraphControl, mGraph);

	VDRegistryAppKey regkey("Audio filters");
	bool bAutoArrange = regkey.getBool("Auto arrange", true);
	bool bAutoConnect = regkey.getBool("Auto connect", true);

	mpGraphControl->EnableAutoArrange(bAutoArrange);
	mpGraphControl->EnableAutoConnect(bAutoConnect);

	CheckDlgButton(mhdlg, IDC_AUTOARRANGE, bAutoArrange);
	CheckDlgButton(mhdlg, IDC_AUTOCONNECT, bAutoConnect);

	EnableWindow(GetDlgItem(mhdlg, IDC_TEST), inputAudio != 0);

	SelectionChanged(NULL);
}

void VDDialogAudioFiltersW32::SaveDialogSettings() {
	VDRegistryAppKey regkey("Audio filters");

	regkey.setBool("Auto arrange", 0!=IsDlgButtonChecked(mhdlg, IDC_AUTOARRANGE));
	regkey.setBool("Auto connect", 0!=IsDlgButtonChecked(mhdlg, IDC_AUTOCONNECT));
}

void VDDisplayAudioFilterDialog(VDGUIHandle hParent, VDAudioFilterGraph& graph) {
	VDDialogAudioFiltersW32 dlg(graph);

	dlg.Activate(hParent);
}

///////////////////////////////////////////////////////////////////////////
