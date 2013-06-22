#include <windows.h>
#include <vfw.h>

#include "resource.h"
#include "List.h"
#include "dub.h"
#include "misc.h"
#include "oshelper.h"
#include "helpfile.h"

struct FilterDefinition;

extern HINSTANCE g_hInst;

const char *const coach_messages[]={
#include "coach.txt"
};

static char disabled_messages[4];
static bool dismsg_valid = false;

struct coachdata {
	const char *msg;
	int id;
};

static BOOL CALLBACK CoachDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLong(hdlg, DWL_USER, lParam);

		SetDlgItemText(hdlg, IDC_MESSAGE, ((struct coachdata *)lParam)->msg);
		return TRUE;

	case WM_COMMAND:
		{
			int code = 0;

			if (IsDlgButtonChecked(hdlg, IDC_DISABLE_THIS_MESSAGE))
				code += 16;

			if (IsDlgButtonChecked(hdlg, IDC_DISABLE_ALL_MESSAGES))
				code += 32;

			switch(LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, code);
				return TRUE;
			case IDCANCEL:
				EndDialog(hdlg, code+1);
				return TRUE;
			case IDHELP:
				HelpPopup(hdlg, IDH_COACH + ((struct coachdata *)GetWindowLong(hdlg, DWL_USER))->id);
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

static const char coach_msg[]="Coach Messages";

static void CoachWarn(HWND hwnd, int id) {
	int ret;
	struct coachdata cd;

	if (!dismsg_valid) {
		int size;

		size = QueryConfigBinary(NULL, coach_msg, disabled_messages, sizeof disabled_messages);
		if (size)
			memset(disabled_messages + size, 0, sizeof disabled_messages - size);
		else {
			memset(disabled_messages, 0, sizeof disabled_messages);
			SetConfigBinary(NULL, coach_msg, disabled_messages, sizeof disabled_messages);
		}

		dismsg_valid = true;
	}
	
	if ((disabled_messages[0] & 1) || (disabled_messages[(id+1)>>3] & (1L<<((id+1)&7))))
		return;

	cd.msg = coach_messages[id];
	cd.id = id;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_COACH), hwnd, CoachDlgProc, (LPARAM)&cd);

	if (ret & 48) {
		if (ret & 16)
			disabled_messages[(id+1) >> 3] |= (1L<<((id+1)&7));

		if (ret & 32)
			disabled_messages[0] |= 1;

		SetConfigBinary(NULL, coach_msg, disabled_messages, sizeof disabled_messages);
	}

	if (ret & 1)
		throw false;
}

bool CoachCheckSaveOp(HWND hwndParent, DubOptions *dopt, COMPVARS *vcomp, WAVEFORMATEX *wfex, List *filter_list) {
	FOURCC fccVpack = 0;

	try {
		if (vcomp->dwFlags & ICMF_COMPVARS_VALID) {
			ICINFO icinfo;
			bool have_info = false;

			if (vcomp->hic) {
				memset(&icinfo, 0, sizeof icinfo);
				icinfo.dwSize = sizeof icinfo;

				if (ICGetInfo(vcomp->hic, &icinfo, sizeof icinfo))
					have_info = true;
			}

			// Trying to use ultra-high quality?

			if (vcomp->lQ > 9000 && have_info && icinfo.dwFlags & VIDCF_QUALITY)
				CoachWarn(hwndParent, 6);

			// No keyframes?

			if (have_info && !vcomp->lKey) {
				if (icinfo.dwFlags & VIDCF_TEMPORAL)
					CoachWarn(hwndParent, 7);
			}

			// Outdated codec?
			// Hacked codec or MPEG-4 V3?

			fccVpack = toupperFOURCC(vcomp->fccHandler);

			switch(fccVpack) {
				case 'MARC': case 'CVSM':	// Microsoft Video 1
				case '23VI':				// Indeo Video R3.2
				case 'DIVC':				// SuperMac/Radius Cinepak
					CoachWarn(hwndParent, 2);
					break;

				// DivX is a direct binary hack.  AngelPotion has the
				// 3688 MS codec buried in its APL file.  Either way,
				// it's a hack.

				case '14PA':				// AngelPotion definitive
				case '2VID':				// DivX codec
				case '3VID':
				case '4VID':
					CoachWarn(hwndParent, 11);
					break;

				case '34PM':
					CoachWarn(hwndParent, 12);
					break;
			}

			// Output depth 16-bit?

			if (fccVpack && dopt->video.outputDepth == DubVideoOptions::D_16BIT)
				CoachWarn(hwndParent, 8);

			// Wrong mode?

			if (fccVpack) {
				if (dopt->video.mode == DubVideoOptions::M_NONE)
					CoachWarn(hwndParent, 1);
				else if (dopt->video.mode == DubVideoOptions::M_FASTREPACK)
					CoachWarn(hwndParent, 10);
			}
		}

		// Filters enabled with wrong mode?

		if (!filter_list->IsEmpty() && dopt->video.mode != DubVideoOptions::M_FULL)
			CoachWarn(hwndParent, 4);

		// Uncompressed video?

		if (!fccVpack && dopt->video.mode >= DubVideoOptions::M_SLOWREPACK)
			CoachWarn(hwndParent, 0);

		if (wfex && wfex->wFormatTag != WAVE_FORMAT_PCM) {

			// Direct Stream Copy audio mode?

			if (dopt->audio.mode == DubAudioOptions::M_NONE)
				CoachWarn(hwndParent, 3);

			// No interleaving?

			if (!dopt->audio.enabled)
				CoachWarn(hwndParent, 9);

		}

	} catch(bool ret) {
		return ret;
	}

	return true;
}
