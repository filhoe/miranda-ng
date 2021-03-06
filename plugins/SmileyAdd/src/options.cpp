/*
Miranda NG SmileyAdd Plugin
Copyright (C) 2012-19 Miranda NG team (https://miranda-ng.org)
Copyright (C) 2005-11 Boris Krasnovskiy All Rights Reserved
Copyright (C) 2003-04 Rein-Peter de Boer

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

OptionsType opt;

#define UM_CHECKSTATECHANGE (WM_USER + 100)

class OptionsDialogType
{
private:
	HWND m_hwndDialog;
	SmileyCategoryListType tmpsmcat;
	SmileyPackType smPack;

	void InitDialog(void);
	void DestroyDialog(void);
	void AddCategory(void);
	void ApplyChanges(void);
	void UpdateControls(bool force = false);
	void SetChanged(void);
	bool BrowseForSmileyPacks(int item);
	void FilenameChanged(void);
	void ShowSmileyPreview(void);
	void PopulateSmPackList(void);
	void UpdateVisibleSmPackList(void);
	void UserAction(HTREEITEM hItem);
	long GetSelProto(HTREEITEM hItem = nullptr);

public:
	OptionsDialogType(HWND hWnd) { m_hwndDialog = hWnd; }
	BOOL DialogProcedure(UINT msg, WPARAM wParam, LPARAM lParam);
};

//OptionsDialog class functions
BOOL OptionsDialogType::DialogProcedure(UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL Result = FALSE;

	switch (msg) {
	case WM_INITDIALOG:
		InitDialog();
		Result = TRUE;
		break;

	case WM_DESTROY:
		DestroyDialog();
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_FILENAME:
			switch (HIWORD(wParam)) {
			case EN_KILLFOCUS:
				FilenameChanged();
				break;

			case EN_CHANGE:
				if (GetFocus() == (HWND)lParam) SetChanged();
				break;
			}
			break;

		case IDC_BROWSE:
			if (HIWORD(wParam) == BN_CLICKED)
				if (BrowseForSmileyPacks(GetSelProto())) {
					UpdateControls(true);
					SetChanged();
				}
			break;

		case IDC_SMLOPTBUTTON:
			if (HIWORD(wParam) == BN_CLICKED)
				ShowSmileyPreview();
			break;

		case IDC_USESTDPACK:
			if (HIWORD(wParam) == BN_CLICKED) {
				BOOL en = IsDlgButtonChecked(m_hwndDialog, IDC_USESTDPACK) == BST_CHECKED;
				EnableWindow(GetDlgItem(m_hwndDialog, IDC_USEPHYSPROTO), en);
			} // no break!
		case IDC_USEPHYSPROTO:
			if (HIWORD(wParam) == BN_CLICKED) {
				PopulateSmPackList();
				SetChanged();
			}
			break;

		case IDC_ADDCATEGORY:
			if (HIWORD(wParam) == BN_CLICKED)
				AddCategory();
			break;

		case IDC_DELETECATEGORY:
			if (HIWORD(wParam) == BN_CLICKED)
				if (tmpsmcat.DeleteCustomCategory(GetSelProto())) {
					PopulateSmPackList();
					SetChanged();
				}
			break;

		case IDC_SPACES:
		case IDC_SCALETOTEXTHEIGHT:
		case IDC_APPENDSPACES:
		case IDC_SCALEALLSMILEYS:
		case IDC_IEVIEWSTYLE:
		case IDC_ANIMATESEL:
		case IDC_ANIMATEDLG:
		case IDC_INPUTSMILEYS:
		case IDC_DCURSORSMILEY:
		case IDC_DISABLECUSTOM:
		case IDC_HQSCALING:
		case IDC_SORTING_HORIZONTAL:
			if (HIWORD(wParam) == BN_CLICKED)
				SetChanged();
			break;

		case IDC_MAXCUSTSMSZ:
		case IDC_MINSMSZ:
			if (HIWORD(wParam) == EN_CHANGE && GetFocus() == (HWND)lParam)
				SetChanged();
			break;
		}
		break;

	case UM_CHECKSTATECHANGE:
		UserAction((HTREEITEM)lParam);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_APPLY:
				ApplyChanges();
				break;
			}
			break;

		case IDC_CATEGORYLIST:
			switch (((LPNMHDR)lParam)->code) {
			case NM_CLICK:
				{
					TVHITTESTINFO ht = { 0 };

					DWORD dwpos = GetMessagePos();
					POINTSTOPOINT(ht.pt, MAKEPOINTS(dwpos));
					MapWindowPoints(HWND_DESKTOP, ((LPNMHDR)lParam)->hwndFrom, &ht.pt, 1);

					TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &ht);
					if (TVHT_ONITEM & ht.flags)
						FilenameChanged();
					if (TVHT_ONITEMSTATEICON & ht.flags)
						PostMessage(m_hwndDialog, UM_CHECKSTATECHANGE, 0, (LPARAM)ht.hItem);
				}

			case TVN_KEYDOWN:
				if (((LPNMTVKEYDOWN)lParam)->wVKey == VK_SPACE)
					PostMessage(m_hwndDialog, UM_CHECKSTATECHANGE, 0,
						(LPARAM)TreeView_GetSelection(((LPNMHDR)lParam)->hwndFrom));
				break;

			case TVN_SELCHANGED:
				LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
				if (pnmtv->itemNew.state & TVIS_SELECTED)
					UpdateControls();
			}
		}
		break;
	}

	return Result;
}

void OptionsDialogType::AddCategory(void)
{
	wchar_t cat[30];
	GetDlgItemText(m_hwndDialog, IDC_NEWCATEGORY, cat, _countof(cat));
	CMStringW catd = cat;

	if (!catd.IsEmpty()) {
		tmpsmcat.AddCategory(cat, catd, smcCustom);

		PopulateSmPackList();
		SetChanged();
	}
}

void OptionsDialogType::UserAction(HTREEITEM hItem)
{
	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);

	if (TreeView_GetCheckState(hLstView, hItem)) {
		if (!BrowseForSmileyPacks(GetSelProto(hItem)))
			TreeView_SetCheckState(hLstView, hItem, TRUE)
	}
	else
		tmpsmcat.GetSmileyCategory(GetSelProto(hItem))->ClearFilename();

	if (hItem == TreeView_GetSelection(hLstView))
		UpdateControls();
	else
		TreeView_SelectItem(hLstView, hItem);

	SetChanged();
	PopulateSmPackList();
}


void OptionsDialogType::SetChanged(void)
{
	SendMessage(GetParent(m_hwndDialog), PSM_CHANGED, 0, 0);
}

void OptionsDialogType::UpdateControls(bool force)
{
	const SmileyCategoryType *smc = tmpsmcat.GetSmileyCategory(GetSelProto());
	if (smc == nullptr)
		return;

	const CMStringW &smf = smc->GetFilename();
	SetDlgItemText(m_hwndDialog, IDC_FILENAME, smf);

	if (smPack.GetFilename() != smf || force)
		smPack.LoadSmileyFile(smf, smPack.GetName(), false, true);

	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);
	TreeView_SetCheckState(hLstView, TreeView_GetSelection(hLstView), smPack.SmileyCount() != 0);

	SetDlgItemText(m_hwndDialog, IDC_LIBAUTHOR, smPack.GetAuthor().c_str());
	SetDlgItemText(m_hwndDialog, IDC_LIBVERSION, smPack.GetVersion().c_str());
	SetDlgItemText(m_hwndDialog, IDC_LIBNAME, TranslateW(smPack.GetName().c_str()));
}

long OptionsDialogType::GetSelProto(HTREEITEM hItem)
{
	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);
	TVITEM tvi = { 0 };

	tvi.mask = TVIF_PARAM;
	tvi.hItem = hItem == nullptr ? TreeView_GetSelection(hLstView) : hItem;

	TreeView_GetItem(hLstView, &tvi);

	return (long)tvi.lParam;
}

void OptionsDialogType::UpdateVisibleSmPackList(void)
{
	bool useOne = IsDlgButtonChecked(m_hwndDialog, IDC_USESTDPACK) == BST_UNCHECKED;
	bool usePhysProto = IsDlgButtonChecked(m_hwndDialog, IDC_USEPHYSPROTO) == BST_CHECKED;

	SmileyCategoryListType::SmileyCategoryVectorType &smc = *tmpsmcat.GetSmileyCategoryList();
	for (auto &it : smc) {
		bool visiblecat = usePhysProto ? !it->IsAcc() : !it->IsPhysProto();
		bool visible = useOne ? !it->IsProto() : visiblecat;

		if (!visible && it->IsAcc() && !useOne) {
			CMStringW PhysProtoName = L"AllProto";
			CMStringW ProtoName = it->GetName();
			DBVARIANT dbv;
			if (db_get_ws(0, _T2A(ProtoName.GetBuffer()), "AM_BaseProto", &dbv) == 0) {
				ProtoName = dbv.pwszVal;
				db_free(&dbv);
			}
			else
				ProtoName.Empty();

			CMStringW FileName;
			if (!ProtoName.IsEmpty()) {
				PhysProtoName += ProtoName;
				SmileyCategoryType *scm = tmpsmcat.GetSmileyCategory(PhysProtoName);
				if (scm == nullptr)
					visible = false;
				else if (scm->GetFilename().IsEmpty())
					visible = true;
			}
		}

		it->SetVisible(visible);
	}
}

void OptionsDialogType::PopulateSmPackList(void)
{
	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);

	TreeView_SelectItem(hLstView, NULL);
	TreeView_DeleteAllItems(hLstView);

	TVINSERTSTRUCT tvi = {};
	tvi.hParent = TVI_ROOT;
	tvi.hInsertAfter = TVI_LAST;
	tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_STATE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
	tvi.item.stateMask = TVIS_STATEIMAGEMASK | TVIS_SELECTED;
	UpdateVisibleSmPackList();
	SmileyCategoryListType::SmileyCategoryVectorType &smc = *tmpsmcat.GetSmileyCategoryList();
	for (int i = 0; i < smc.getCount(); i++) {
		if (smc[i].IsVisible()) {
			tvi.item.pszText = (wchar_t*)smc[i].GetDisplayName().c_str();
			if (!smc[i].IsProto()) {
				tvi.item.iImage = 0;
				tvi.item.iSelectedImage = 0;
			}
			else {
				tvi.item.iImage = i;
				tvi.item.iSelectedImage = i;
			}
			tvi.item.lParam = i;
			tvi.item.state = INDEXTOSTATEIMAGEMASK(smPack.LoadSmileyFile(smc[i].GetFilename(), smc[i].GetDisplayName(), true, true) ? 2 : 1);
			TreeView_InsertItem(hLstView, &tvi);

			smPack.Clear();
		}
	}
	TreeView_SelectItem(hLstView, TreeView_GetRoot(hLstView));
}

void OptionsDialogType::InitDialog(void)
{
	TranslateDialogDefault(m_hwndDialog);

	CheckDlgButton(m_hwndDialog, IDC_SPACES, opt.EnforceSpaces ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_SCALETOTEXTHEIGHT, opt.ScaleToTextheight ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_USESTDPACK, opt.UseOneForAll ? BST_UNCHECKED : BST_CHECKED);
	CheckDlgButton(m_hwndDialog, IDC_USEPHYSPROTO, opt.UsePhysProto ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_APPENDSPACES, opt.SurroundSmileyWithSpaces ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_SCALEALLSMILEYS, opt.ScaleAllSmileys ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_IEVIEWSTYLE, opt.IEViewStyle ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_ANIMATESEL, opt.AnimateSel ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_ANIMATEDLG, opt.AnimateDlg ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_INPUTSMILEYS, opt.InputSmileys ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_DCURSORSMILEY, opt.DCursorSmiley ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_DISABLECUSTOM, opt.DisableCustom ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_HQSCALING, opt.HQScaling ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hwndDialog, IDC_SORTING_HORIZONTAL, opt.HorizontalSorting ? BST_CHECKED : BST_UNCHECKED);

	EnableWindow(GetDlgItem(m_hwndDialog, IDC_USEPHYSPROTO), !opt.UseOneForAll);

	SendDlgItemMessage(m_hwndDialog, IDC_MAXCUSTSPIN, UDM_SETRANGE32, 0, 99);
	SendDlgItemMessage(m_hwndDialog, IDC_MAXCUSTSPIN, UDM_SETPOS, 0, opt.MaxCustomSmileySize);
	SendDlgItemMessage(m_hwndDialog, IDC_MAXCUSTSMSZ, EM_LIMITTEXT, 2, 0);

	SendDlgItemMessage(m_hwndDialog, IDC_MINSPIN, UDM_SETRANGE32, 0, 99);
	SendDlgItemMessage(m_hwndDialog, IDC_MINSPIN, UDM_SETPOS, 0, opt.MinSmileySize);
	SendDlgItemMessage(m_hwndDialog, IDC_MINSMSZ, EM_LIMITTEXT, 2, 0);

	// Create and populate image list
	HIMAGELIST hImList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
		ILC_MASK | ILC_COLOR32, g_SmileyCategories.NumberOfSmileyCategories(), 0);

	tmpsmcat = g_SmileyCategories;

	SmileyCategoryListType::SmileyCategoryVectorType &smc = *g_SmileyCategories.GetSmileyCategoryList();
	for (auto &it : smc) {
		HICON hIcon = nullptr;
		if (it->IsProto()) {
			hIcon = (HICON)CallProtoService(_T2A(it->GetName().c_str()), PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
			if (hIcon == nullptr || (INT_PTR)hIcon == CALLSERVICE_NOTFOUND)
				hIcon = (HICON)CallProtoService(_T2A(it->GetName().c_str()), PS_LOADICON, PLI_PROTOCOL, 0);
		}
		if (hIcon == nullptr || hIcon == (HICON)CALLSERVICE_NOTFOUND)
			hIcon = GetDefaultIcon();

		ImageList_AddIcon(hImList, hIcon);
		DestroyIcon(hIcon);
	}

	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);
	TreeView_SetImageList(hLstView, hImList, TVSIL_NORMAL);

	PopulateSmPackList();
}

void OptionsDialogType::DestroyDialog(void)
{
	HWND hLstView = GetDlgItem(m_hwndDialog, IDC_CATEGORYLIST);
	HIMAGELIST hImList = TreeView_SetImageList(hLstView, NULL, TVSIL_NORMAL);
	ImageList_Destroy(hImList);
}

void OptionsDialogType::ApplyChanges(void)
{
	ProcessAllInputAreas(true);
	CloseSmileys();

	opt.EnforceSpaces = IsDlgButtonChecked(m_hwndDialog, IDC_SPACES) == BST_CHECKED;
	opt.ScaleToTextheight = IsDlgButtonChecked(m_hwndDialog, IDC_SCALETOTEXTHEIGHT) == BST_CHECKED;
	opt.UseOneForAll = IsDlgButtonChecked(m_hwndDialog, IDC_USESTDPACK) == BST_UNCHECKED;
	opt.UsePhysProto = IsDlgButtonChecked(m_hwndDialog, IDC_USEPHYSPROTO) == BST_CHECKED;
	opt.SurroundSmileyWithSpaces = IsDlgButtonChecked(m_hwndDialog, IDC_APPENDSPACES) == BST_CHECKED;
	opt.ScaleAllSmileys = IsDlgButtonChecked(m_hwndDialog, IDC_SCALEALLSMILEYS) == BST_CHECKED;
	opt.IEViewStyle = IsDlgButtonChecked(m_hwndDialog, IDC_IEVIEWSTYLE) == BST_CHECKED;
	opt.AnimateSel = IsDlgButtonChecked(m_hwndDialog, IDC_ANIMATESEL) == BST_CHECKED;
	opt.AnimateDlg = IsDlgButtonChecked(m_hwndDialog, IDC_ANIMATEDLG) == BST_CHECKED;
	opt.InputSmileys = IsDlgButtonChecked(m_hwndDialog, IDC_INPUTSMILEYS) == BST_CHECKED;
	opt.DCursorSmiley = IsDlgButtonChecked(m_hwndDialog, IDC_DCURSORSMILEY) == BST_CHECKED;
	opt.DisableCustom = IsDlgButtonChecked(m_hwndDialog, IDC_DISABLECUSTOM) == BST_CHECKED;
	opt.HQScaling = IsDlgButtonChecked(m_hwndDialog, IDC_HQSCALING) == BST_CHECKED;
	opt.HorizontalSorting = IsDlgButtonChecked(m_hwndDialog, IDC_SORTING_HORIZONTAL) == BST_CHECKED;

	opt.MaxCustomSmileySize = GetDlgItemInt(m_hwndDialog, IDC_MAXCUSTSMSZ, nullptr, FALSE);
	opt.MinSmileySize = GetDlgItemInt(m_hwndDialog, IDC_MINSMSZ, nullptr, FALSE);

	opt.Save();

	// Cleanup database
	CMStringW empty;
	SmileyCategoryListType::SmileyCategoryVectorType &smc = *g_SmileyCategories.GetSmileyCategoryList();
	for (auto &it : smc)
		if (tmpsmcat.GetSmileyCategory(it->GetName()) == nullptr)
			opt.WritePackFileName(empty, it->GetName());

	g_SmileyCategories = tmpsmcat;
	g_SmileyCategories.SaveSettings();
	g_SmileyCategories.ClearAndLoadAll();

	smPack.LoadSmileyFile(tmpsmcat.GetSmileyCategory(GetSelProto())->GetFilename(), tmpsmcat.GetSmileyCategory(GetSelProto())->GetDisplayName(), false, true);

	NotifyEventHooks(hEvent1, 0, 0);
	ProcessAllInputAreas(false);
}

bool OptionsDialogType::BrowseForSmileyPacks(int item)
{
	OPENFILENAME ofn = { 0 };

	wchar_t filename[MAX_PATH] = L"";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = _countof(filename);

	CMStringW inidir;
	SmileyCategoryType *smc = tmpsmcat.GetSmileyCategory(item);
	if (smc->GetFilename().IsEmpty())
		pathToAbsolute(L"Smileys", inidir);
	else {
		pathToAbsolute(smc->GetFilename(), inidir);
		inidir.Truncate(inidir.ReverseFind('\\'));
	}

	ofn.lpstrInitialDir = inidir.c_str();
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = m_hwndDialog;

	wchar_t filter[512], *pfilter;
	mir_wstrcpy(filter, TranslateT("Smiley packs"));
	mir_wstrcat(filter, L" (*.msl;*.asl;*.xep)");
	pfilter = filter + mir_wstrlen(filter) + 1;
	mir_wstrcpy(pfilter, L"*.msl;*.asl;*.xep");
	pfilter = pfilter + mir_wstrlen(pfilter) + 1;
	mir_wstrcpy(pfilter, TranslateT("All files"));
	mir_wstrcat(pfilter, L" (*.*)");
	pfilter = pfilter + mir_wstrlen(pfilter) + 1;
	mir_wstrcpy(pfilter, L"*.*");
	pfilter = pfilter + mir_wstrlen(pfilter) + 1;
	*pfilter = '\0';
	ofn.lpstrFilter = filter;

	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_READONLY |
		OFN_EXPLORER | OFN_LONGNAMES | OFN_NOCHANGEDIR;
	ofn.lpstrDefExt = L"msl";

	if (GetOpenFileName(&ofn)) {
		CMStringW relpath;
		pathToRelative(filename, relpath);
		smc->SetFilename(relpath);

		return true;
	}

	return false;
}

void OptionsDialogType::FilenameChanged(void)
{
	wchar_t str[MAX_PATH];
	GetDlgItemText(m_hwndDialog, IDC_FILENAME, str, _countof(str));

	SmileyCategoryType *smc = tmpsmcat.GetSmileyCategory(GetSelProto());
	if (smc->GetFilename() != str) {
		CMStringW temp(str);
		smc->SetFilename(temp);
		UpdateControls();
	}
}

void OptionsDialogType::ShowSmileyPreview(void)
{
	RECT rect;
	GetWindowRect(GetDlgItem(m_hwndDialog, IDC_SMLOPTBUTTON), &rect);

	SmileyToolWindowParam *stwp = new SmileyToolWindowParam;
	stwp->pSmileyPack = &smPack;
	stwp->hWndParent = m_hwndDialog;
	stwp->hWndTarget = nullptr;
	stwp->targetMessage = 0;
	stwp->targetWParam = 0;
	stwp->xPosition = rect.left;
	stwp->yPosition = rect.bottom + 4;
	stwp->direction = 1;
	stwp->hContact = NULL;

	mir_forkThread<SmileyToolWindowParam>(SmileyToolThread, stwp);
}

void OptionsType::Save(void)
{
	g_plugin.setByte("EnforceSpaces", EnforceSpaces);
	g_plugin.setByte("ScaleToTextheight", ScaleToTextheight);
	g_plugin.setByte("UseOneForAll", UseOneForAll);
	g_plugin.setByte("UsePhysProto", UsePhysProto);
	g_plugin.setByte("SurroundSmileyWithSpaces", SurroundSmileyWithSpaces);
	g_plugin.setByte("ScaleAllSmileys", ScaleAllSmileys);
	g_plugin.setByte("IEViewStyle", IEViewStyle);
	g_plugin.setByte("AnimateSel", AnimateSel);
	g_plugin.setByte("AnimateDlg", AnimateDlg);
	g_plugin.setByte("InputSmileys", InputSmileys);
	g_plugin.setByte("DCursorSmiley", DCursorSmiley);
	g_plugin.setByte("DisableCustom", DisableCustom);
	g_plugin.setByte("HQScaling", HQScaling);
	g_plugin.setDword("MaxCustomSmileySize", MaxCustomSmileySize);
	g_plugin.setDword("MinSmileySize", MinSmileySize);
	g_plugin.setByte("HorizontalSorting", HorizontalSorting);
}

void OptionsType::Load(void)
{
	EnforceSpaces = g_plugin.getByte("EnforceSpaces", FALSE) != 0;
	ScaleToTextheight = g_plugin.getByte("ScaleToTextheight", FALSE) != 0;
	UseOneForAll = g_plugin.getByte("UseOneForAll", TRUE) != 0;
	UsePhysProto = g_plugin.getByte("UsePhysProto", FALSE) != 0;
	SurroundSmileyWithSpaces = g_plugin.getByte("SurroundSmileyWithSpaces", FALSE) != 0;
	ScaleAllSmileys = g_plugin.getByte("ScaleAllSmileys", FALSE) != 0;
	IEViewStyle = g_plugin.getByte("IEViewStyle", FALSE) != 0;
	AnimateSel = g_plugin.getByte("AnimateSel", TRUE) != 0;
	AnimateDlg = g_plugin.getByte("AnimateDlg", TRUE) != 0;
	InputSmileys = g_plugin.getByte("InputSmileys", TRUE) != 0;
	DCursorSmiley = g_plugin.getByte("DCursorSmiley", FALSE) != 0;
	DisableCustom = g_plugin.getByte("DisableCustom", FALSE) != 0;
	HQScaling = g_plugin.getByte("HQScaling", FALSE) != 0;

	SelWndBkgClr = g_plugin.getDword("SelWndBkgClr", GetSysColor(COLOR_WINDOW));
	MaxCustomSmileySize = g_plugin.getDword("MaxCustomSmileySize", 0);
	MinSmileySize = g_plugin.getDword("MinSmileySize", 0);
	HorizontalSorting = g_plugin.getByte("HorizontalSorting", 1) != 0;
}

void OptionsType::ReadPackFileName(CMStringW &filename, const CMStringW &name, const CMStringW &defaultFilename)
{
	CMStringW settingKey = name + L"-filename";

	ptrW tszValue(g_plugin.getWStringA(_T2A(settingKey.c_str())));
	filename = (tszValue != NULL) ? (wchar_t*)tszValue : defaultFilename;
}

void OptionsType::WritePackFileName(const CMStringW &filename, const CMStringW &name)
{
	CMStringW settingKey = name + L"-filename";
	g_plugin.setWString(_T2A(settingKey.c_str()), filename.c_str());
}

void OptionsType::ReadCustomCategories(CMStringW &cats)
{
	ptrW tszValue(g_plugin.getWStringA("CustomCategories"));
	if (tszValue != NULL)
		cats = tszValue;
}

void OptionsType::WriteCustomCategories(const CMStringW &cats)
{
	if (cats.IsEmpty())
		g_plugin.delSetting("CustomCategories");
	else
		g_plugin.setWString("CustomCategories", cats.c_str());
}

void OptionsType::ReadContactCategory(MCONTACT hContact, CMStringW &cats)
{
	ptrW tszValue(g_plugin.getWStringA(hContact, "CustomCategory"));
	if (tszValue != NULL)
		cats = tszValue;
}

void OptionsType::WriteContactCategory(MCONTACT hContact, const CMStringW &cats)
{
	if (cats.IsEmpty())
		g_plugin.delSetting(hContact, "CustomCategory");
	else
		g_plugin.setWString(hContact, "CustomCategory", cats.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////
// Init and de-init functions, called from main

static INT_PTR CALLBACK DlgProcSmileysOptions(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	OptionsDialogType *pOD = (OptionsDialogType*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	if (pOD == nullptr) {
		pOD = new OptionsDialogType(hwndDlg);
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)pOD);
	}

	INT_PTR Result = pOD->DialogProcedure(msg, wParam, lParam);
	SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, Result);

	if (msg == WM_NCDESTROY)
		delete pOD;

	return Result;
}

int SmileysOptionsInitialize(WPARAM addInfo, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.position = 910000000;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_SMILEYS);
	odp.szTitle.a = LPGEN("Smileys");
	odp.szGroup.a = LPGEN("Customize");
	odp.pfnDlgProc = DlgProcSmileysOptions;
	odp.flags = ODPF_BOLDGROUPS;
	g_plugin.addOptions(addInfo, &odp);
	return 0;
}
