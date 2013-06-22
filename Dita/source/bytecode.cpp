#ifdef _MSC_VER
	#pragma warning(disable: 4786)		// shut up
#endif

#include <vector>

#include <vd2/Dita/interface.h>
#include <vd2/Dita/bytecode.h>
#include <vd2/Dita/resources.h>

namespace {
	union StackElem {
		int i;
		const wchar_t *s;

		StackElem() {}
		StackElem(int _i) : i(_i) {}
		StackElem(const wchar_t *_s) : s(_s) {}
	};
};

void VDExecuteDialogResource(const unsigned char *pBytecode, IVDUIBase *pBase, IVDUIConstructor *pConstructor) {
	using namespace nsVDDitaBytecode;

	std::vector<StackElem> stack;

	for(;;) {
		int sp = stack.size();

		switch(*pBytecode++) {
		case kBC_End:			return;
		case kBC_Zero:			stack.push_back(0); break;
		case kBC_One:			stack.push_back(1); break;
		case kBC_Int8:			stack.push_back((signed char)*pBytecode++); break;
		case kBC_Int32:			stack.push_back(*(int *)pBytecode); pBytecode += 4; break;
		case kBC_String:		stack.push_back(VDLoadString(0, ((short *)pBytecode)[0], ((short *)pBytecode)[1])); pBytecode += 4; break;
		case kBC_StringShort:	stack.push_back(VDLoadString(0, 0xffff, *(short *)pBytecode)); pBytecode += 2; break;
		case kBC_InvokeTemplate:
			VDExecuteDialogResource(VDLoadTemplate(0, *(short *)pBytecode), pBase, pConstructor);
			pBytecode += 2;
			break;

		case kBC_CreateLabel:
			pConstructor->AddLabel(stack[sp-3].i, stack[sp-2].s, stack[sp-1].i);
			stack.resize(sp-3);
			break;
		case kBC_CreateEdit:
			pConstructor->AddEdit(stack[sp-3].i, stack[sp-2].s, stack[sp-1].i);
			stack.resize(sp-3);
			break;
		case kBC_CreateEditInt:
			pConstructor->AddEditInt(stack[sp-4].i, stack[sp-3].i, stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-4);
			break;
		case kBC_CreateButton:
			pConstructor->AddButton(stack[sp-2].i, stack[sp-1].s);
			stack.resize(sp-2);
			break;
		case kBC_CreateCheckBox:
			pConstructor->AddCheckbox(stack[sp-2].i, stack[sp-1].s);
			stack.resize(sp-2);
			break;
		case kBC_CreateListBox:
			pConstructor->AddListbox(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_CreateComboBox:
			pConstructor->AddCombobox(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_CreateListView:
			pConstructor->AddListView(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_CreateTrackbar:
			pConstructor->AddTrackbar(stack[sp-3].i, stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-3);
			break;
		case kBC_CreateFileControl:
			pConstructor->AddFileControl(stack[sp-2].i, L"", stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_CreateOptionSet:
			pConstructor->BeginOptionSet(stack[sp-1].i);
			stack.resize(sp-1);
			break;
		case kBC_AddOption:
			pConstructor->AddOption(stack[sp-1].s);
			stack.resize(sp-1);
			break;
		case kBC_EndOptionSet:
			pConstructor->EndOptionSet();
			break;
		case kBC_CreateHorizSet:
			pConstructor->BeginHorizSet(stack[sp-1].i);
			stack.resize(sp-1);
			break;
		case kBC_CreateVertSet:
			pConstructor->BeginVertSet(stack[sp-1].i);
			stack.resize(sp-1);
			break;
		case kBC_CreateGroupSet:
			pConstructor->BeginGroupSet(stack[sp-2].i, stack[sp-1].s);
			stack.resize(sp-2);
			break;
		case kBC_EndSet:
			pConstructor->EndSet();
			break;
		case kBC_CreateGrid:
			pConstructor->BeginGrid(stack[sp-6].i, stack[sp-5].i, stack[sp-4].i, stack[sp-3].i, stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-6);
			break;
		case kBC_EndGrid:
			pConstructor->EndGrid();
			break;
		case kBC_SetGridDirection:
			pConstructor->SetGridDirection((nsVDUI::eGridDirection)stack[sp-1].i);
			stack.resize(sp-1);
			break;
		case kBC_SpanNext:
			pConstructor->SpanNext(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SkipGrid:
			pConstructor->SkipGrid(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetGridPos:
			pConstructor->SetGridPos(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetColumnAffinity:
			pConstructor->SetGridColumnAffinity(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetRowAffinity:
			pConstructor->SetGridRowAffinity(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetDialogInfo:
			pBase->AsControl()->SetMinimumSize(VDUISize(stack[sp-4].i, stack[sp-3].i));
			pBase->AsControl()->SetDesiredAspectRatio(stack[sp-2].i / 100.0);
			pBase->AsControl()->SetTextw(stack[sp-1].s);
			stack.resize(sp-4);
			break;
		case kBC_SetAlignment:
			pConstructor->SetAlignment(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetMinSize:
			pConstructor->SetMinimumSize(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_SetMaxSize:
			pConstructor->SetMaximumSize(stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		case kBC_Link:
			pBase->Link(stack[sp-3].i, (nsVDUI::eLinkType)stack[sp-2].i, stack[sp-1].i);
			stack.resize(sp-3);
			break;
		case kBC_AddColumn:
			pConstructor->GetLastControl()->AsUIList()->AddColumn(stack[sp-2].s, stack[sp-1].i);
			stack.resize(sp-2);
			break;
		default:
			VDNEVERHERE;
		}
	}

	VDASSERT(stack.empty());
}
