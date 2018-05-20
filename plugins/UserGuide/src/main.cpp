#include "stdafx.h"

CMPlugin g_plugin;
int &hLangpack(g_plugin.m_hLang);

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// 297EC1E7-41B7-41F9-BB91-EFA95028F16C
	{ 0x297ec1e7, 0x41b7, 0x41f9, {0xbb, 0x91, 0xef, 0xa9, 0x50, 0x28, 0xf1, 0x6c }}
};

CMPlugin::CMPlugin() :
	PLUGIN<CMPlugin>(nullptr, pluginInfoEx)
{}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfoEx;
}

/////////////////////////////////////////////////////////////////////////////////////////

static INT_PTR ShowGuideFile(WPARAM, LPARAM)
{
	LPTSTR pszDirName = (LPTSTR)mir_alloc(250 * sizeof(wchar_t));
	LPTSTR pszFileName = (LPTSTR)mir_alloc(250 * sizeof(wchar_t));

	wchar_t *ptszHelpFile = db_get_wsa(NULL, "UserGuide", "PathToHelpFile");

	if (ptszHelpFile == nullptr) {
		mir_wstrcpy(pszDirName, L"%miranda_path%\\Plugins");
		mir_wstrcpy(pszFileName, L"UserGuide.chm");
	}
	else {
		if (!mir_wstrcmp(ptszHelpFile, L"")) {
			mir_wstrcpy(pszDirName, L"%miranda_path%\\Plugins");
			mir_wstrcpy(pszFileName, L"UserGuide.chm");
		}
		else {
			LPTSTR pszDivider = wcsrchr(ptszHelpFile, '\\');
			if (pszDivider == nullptr) {
				mir_wstrcpy(pszDirName, L"");
				wcsncpy(pszFileName, ptszHelpFile, mir_wstrlen(ptszHelpFile));
			}
			else {
				wcsncpy(pszFileName, pszDivider + 1, mir_wstrlen(ptszHelpFile) - mir_wstrlen(pszDivider) - 1);
				pszFileName[mir_wstrlen(ptszHelpFile) - mir_wstrlen(pszDivider) - 1] = 0;
				wcsncpy(pszDirName, ptszHelpFile, pszDivider - ptszHelpFile);
				pszDirName[pszDivider - ptszHelpFile] = 0;
			}
		}
		mir_free(ptszHelpFile);
	}
	LPTSTR pszDirNameEx;
	pszDirNameEx = Utils_ReplaceVarsW(pszDirName);
	mir_free(pszDirName);

	ShellExecute(nullptr, L"open", pszFileName, nullptr, pszDirNameEx, SW_SHOW);
	mir_free(pszFileName);
	mir_free(pszDirNameEx);
	return 0;
}

extern "C" __declspec(dllexport) int Load(void)
{
	CMenuItem mi;
	SET_UID(mi, 0x6787c12d, 0xdc85, 0x409d, 0xaa, 0x6c, 0x1f, 0xfe, 0x5f, 0xe8, 0xc1, 0x18);
	mi.position = 500000;
	mi.flags = CMIF_UNICODE;
	mi.hIcolibItem = Skin_LoadIcon(SKINICON_OTHER_HELP);
	mi.name.w = LPGENW("User Guide");
	mi.pszService = "UserGuide/ShowGuide";
	Menu_AddMainMenuItem(&mi);

	CreateServiceFunction(mi.pszService, ShowGuideFile);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) int Unload(void)
{
	return 0;
}
