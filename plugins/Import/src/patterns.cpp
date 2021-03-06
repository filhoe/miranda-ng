/*

Import plugin for Miranda NG

Copyright (C) 2012-19 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "stdafx.h"

#include <memory>
#include <vector>

void CMPlugin::LoadPatterns()
{
	wchar_t wszPath[MAX_PATH], wszFullPath[MAX_PATH];
	GetModuleFileNameW(m_hInst, wszPath, _countof(wszPath));
	if (auto *p = wcsrchr(wszPath, '\\'))
		*p = 0;
	
	mir_snwprintf(wszFullPath, L"%s\\Import\\*.ini", wszPath);

	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(wszFullPath, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do {
		mir_snwprintf(wszFullPath, L"%s\\Import\\%s", wszPath, fd.cFileName);
		LoadPattern(wszFullPath);
	}
	while (FindNextFileW(hFind, &fd) != 0);
}

void CMPlugin::LoadPattern(const wchar_t *pwszFileName)
{
	// [General] section
	wchar_t buf[1024];
	if (!GetPrivateProfileStringW(L"General", L"Name", L"", buf, _countof(buf), pwszFileName))
		return;

	std::auto_ptr<CImportPattern> pNew(new CImportPattern());
	pNew->wszName = buf;
	pNew->iType = GetPrivateProfileIntW(L"General", L"Type", 1, pwszFileName);

	if (GetPrivateProfileStringW(L"General", L"DefaultExtension", L"", buf, _countof(buf), pwszFileName))
		pNew->wszExt = buf;

	if (GetPrivateProfileStringW(L"General", L"Charset", L"", buf, _countof(buf), pwszFileName)) {
		if (!wcsicmp(buf, L"ANSI"))
			pNew->iCodePage = GetPrivateProfileIntW(L"General", L"Codepage", CP_ACP, pwszFileName);
		else if (!wcsicmp(buf, L"UCS2"))
			pNew->iCodePage = 1200;
	}
	else return;

	pNew->iUseHeader = GetPrivateProfileIntW(L"General", L"UseHeader", 0, pwszFileName);
	pNew->iUsePreMsg = GetPrivateProfileIntW(L"General", L"UsePreMsg", 0, pwszFileName);
	pNew->iUseFilename = GetPrivateProfileIntW(L"General", L"UseFileName", 0, pwszFileName);

	// [Message] section
	int erroffset;
	const char *err;
	if (GetPrivateProfileStringW(L"Message", L"Pattern", L"", buf, _countof(buf), pwszFileName)) {
		if ((pNew->regMessage.pattern = pcre16_compile(buf, PCRE_MULTILINE, &err, &erroffset, nullptr)) == nullptr)
			return;
		pNew->regMessage.extra = pcre16_study(pNew->regMessage.pattern, 0, &err);
	}
	else return;

	if (GetPrivateProfileStringW(L"Message", L"In", L"", buf, _countof(buf), pwszFileName))
		pNew->wszIncoming = buf;
	if (GetPrivateProfileStringW(L"Message", L"Out", L"", buf, _countof(buf), pwszFileName))
		pNew->wszOutgoing = buf;

	pNew->iDirection = GetPrivateProfileIntW(L"Message", L"Direction", 0, pwszFileName);
	pNew->iDay = GetPrivateProfileIntW(L"Message", L"Day", 0, pwszFileName);
	pNew->iMonth = GetPrivateProfileIntW(L"Message", L"Month", 0, pwszFileName);
	pNew->iYear = GetPrivateProfileIntW(L"Message", L"Year", 0, pwszFileName);
	pNew->iHours = GetPrivateProfileIntW(L"Message", L"Hours", 0, pwszFileName);
	pNew->iMinutes = GetPrivateProfileIntW(L"Message", L"Minutes", 0, pwszFileName);
	pNew->iSeconds = GetPrivateProfileIntW(L"Message", L"Seconds", 0, pwszFileName);

	if (pNew->iUsePreMsg) {
		pNew->preRN = GetPrivateProfileIntW(L"PreMessage", L"PreRN", -1, pwszFileName);
		pNew->preSP = GetPrivateProfileIntW(L"PreMessage", L"PreSP", 0, pwszFileName);
		pNew->afterRN = GetPrivateProfileIntW(L"PreMessage", L"AfterRN", -1, pwszFileName);
		pNew->afterSP = GetPrivateProfileIntW(L"PreMessage", L"AfterSP", 0, pwszFileName);
	}

	if (pNew->iUseFilename) {
		if (!GetPrivateProfileStringW(L"FileName", L"Pattern", L"", buf, _countof(buf), pwszFileName))
			return;
		if ((pNew->regFilename.pattern = pcre16_compile(buf, 0, &err, &erroffset, nullptr)) == nullptr)
			return;
		pNew->regFilename.extra = pcre16_study(pNew->regFilename.pattern, 0, &err);

		pNew->iInNick = GetPrivateProfileIntW(L"FileName", L"InNick", 0, pwszFileName);
		pNew->iInUID = GetPrivateProfileIntW(L"FileName", L"InUID", 0, pwszFileName);
		pNew->iOutNick = GetPrivateProfileIntW(L"FileName", L"OutNick", 0, pwszFileName);
		pNew->iOutUID = GetPrivateProfileIntW(L"FileName", L"OutUID", 0, pwszFileName);
	}

	m_patterns.insert(pNew.release());
}

/////////////////////////////////////////////////////////////////////////////////////////
// pattern-based database driver

class CDbxPattern : public MDatabaseReadonly, public MZeroedObject
{
	HANDLE m_hFile = INVALID_HANDLE_VALUE, m_hMap = nullptr;
	uint32_t m_iFileLen;
	char* m_pFile;
	CMStringW m_buf;

	std::vector<DWORD> m_events;

public:
	CDbxPattern()
	{}

	~CDbxPattern()
	{
	}

	void Load()
	{
		// mcontacts operates with the only contact with pseudo id=1
		m_cache->AddContactToCache(1);

		switch (g_pActivePattern->iCodePage) {
		case CP_UTF8:
			m_buf = mir_utf8decodeW(m_pFile);
			break;
		case 1200:
			m_buf = mir_wstrdup((wchar_t*)m_pFile);
			break;
		default:
			m_buf = mir_a2u_cp(m_pFile, g_pActivePattern->iCodePage);
			break;
		}

		if (m_buf.IsEmpty())
			return;

		int iOffset = 0;
		while (true) {
			int offsets[99];
			int nMatch = pcre16_exec(g_pActivePattern->regMessage.pattern, g_pActivePattern->regMessage.extra, m_buf, m_buf.GetLength(), iOffset, PCRE_NEWLINE_ANYCRLF, offsets, _countof(offsets));
			if (nMatch <= 0)
				break;

			m_events.push_back(offsets[0]);
			iOffset = offsets[1];
		}

		if (m_pFile != nullptr)
			::UnmapViewOfFile(m_pFile);

		if (m_hMap != nullptr)
			::CloseHandle(m_hMap);

		if (m_hFile != INVALID_HANDLE_VALUE)
			::CloseHandle(m_hFile);
	}

	int Open(const wchar_t *profile)
	{
		m_hFile = ::CreateFileW(profile, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, 0);
		if (m_hFile == INVALID_HANDLE_VALUE) {
			Netlib_Logf(0, "failed to open file <%S> for import: %d", profile, GetLastError());
			return EGROKPRF_CANTREAD;
		}

		m_iFileLen = ::GetFileSize(m_hFile, 0);
		m_hMap = ::CreateFileMappingW(m_hFile, nullptr, PAGE_READONLY, 0, 0, L"ImportMapfile");
		if (m_hMap == nullptr) {
			Netlib_Logf(0, "failed to mmap file <%S> for import: %d", profile, GetLastError());
			return EGROKPRF_CANTREAD;
		}

		m_pFile = (char*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
		if (m_pFile == nullptr) {
			Netlib_Logf(0, "failed to map view of file <%S> for import: %d", profile, GetLastError());
			return EGROKPRF_CANTREAD;
		}

		return EGROKPRF_NOERROR;
	}
	
	// patterns file always stores history for the single contact only
	STDMETHODIMP_(LONG) GetBlobSize(MEVENT idx) override
	{
		if (m_events.size() == 0 || idx < 1 || idx > m_events.size())
			return 0;

		int iStart = m_events[idx-1], iEnd = (idx == m_events.size()) ? m_buf.GetLength() : m_events[idx];
		CMStringW msg = m_buf.Mid(iStart, iEnd - iStart);
		return (LONG)mir_strlen(ptrA(mir_utf8encodeW(msg))) + 1;
	}
	
	STDMETHODIMP_(LONG) GetContactCount(void) override
	{
		return 1;
	}

	STDMETHODIMP_(LONG) GetEventCount(MCONTACT) override
	{
		return (LONG)m_events.size();
	}

	////////////////////////////////////////////////////////////////////////////////////////

	static int str2int(const wchar_t* str)
	{
		if (str == nullptr || *str == 0)
			return 0;

		return _wtoi(str);
	}

	STDMETHODIMP_(BOOL) GetEvent(MEVENT idx, DBEVENTINFO *dbei) override
	{
		if (m_events.size() == 0 || idx < 1 || idx > m_events.size())
			return 1;

		int offsets[99];
		int nMatch = pcre16_exec(g_pActivePattern->regMessage.pattern, g_pActivePattern->regMessage.extra, m_buf, m_buf.GetLength(), m_events[idx - 1], PCRE_NEWLINE_ANYCRLF, offsets, _countof(offsets));
		if (nMatch <= 0)
			return 1;

		dbei->eventType = EVENTTYPE_MESSAGE;
		dbei->flags = DBEF_READ | DBEF_UTF;

		int h1 = offsets[1], h2 = (idx == m_events.size()) ? m_buf.GetLength() : m_events[idx];
		int prn = -1, arn = -1;
		if (g_pActivePattern->iUsePreMsg)
			prn = g_pActivePattern->preRN, arn = g_pActivePattern->afterRN;

		if (prn != 0) {
			int i = 0;
			while (m_buf[h1] == '\r' && m_buf[h1 + 1] == '\n' && i < prn)
				h1 += 2, i++;
		}

		if (arn != 0) {
			int i = 0;
			while (m_buf[h2-2] == '\r' && m_buf[h2 - 1] == '\n' && i < arn)
				h2 -= 2, i++;
		}

		if (dbei->cbBlob) {
			CMStringW wszBody = m_buf.Mid(h1, h2-h1).Trim();
			if (!wszBody.IsEmpty()) {
				ptrA tmp(mir_utf8encodeW(wszBody));
				int copySize = min(dbei->cbBlob - 1, (int)mir_strlen(tmp));
				memcpy(dbei->pBlob, tmp, copySize);
				dbei->pBlob[copySize] = 0;
				dbei->cbBlob = copySize;
			}
			else dbei->cbBlob = 0;
		}

		const wchar_t **substrings;
		if (pcre16_get_substring_list(m_buf, offsets, nMatch, &substrings) >= 0) {
			struct tm st = {};
			st.tm_year = str2int(substrings[g_pActivePattern->iYear]);
			if (st.tm_year > 1900)
				st.tm_year -= 1900;
			st.tm_mon = str2int(substrings[g_pActivePattern->iMonth]) - 1;
			st.tm_mday = str2int(substrings[g_pActivePattern->iDay]);
			st.tm_hour = str2int(substrings[g_pActivePattern->iHours]);
			st.tm_min = str2int(substrings[g_pActivePattern->iMinutes]);
			st.tm_sec = (g_pActivePattern->iSeconds) ? str2int(substrings[g_pActivePattern->iSeconds]) : 0;
			dbei->timestamp = mktime(&st);

			if (g_pActivePattern->iDirection)
				if (g_pActivePattern->wszOutgoing == substrings[g_pActivePattern->iDirection])
					dbei->flags |= DBEF_SENT;


			pcre16_free_substring_list(substrings);
		}

		return 0;
	}

	STDMETHODIMP_(MEVENT) FindFirstEvent(MCONTACT) override
	{
		return m_events.size() > 0 ? 1 : 0;
	}

	STDMETHODIMP_(MEVENT) FindNextEvent(MCONTACT, MEVENT idx) override
	{
		if (idx >= m_events.size())
			return 0;

		return idx + 1;
	}

	STDMETHODIMP_(MEVENT) FindLastEvent(MCONTACT) override
	{
		return m_events.size() > 0 ? (MEVENT)m_events.size() : 0;
	}

	STDMETHODIMP_(MEVENT) FindPrevEvent(MCONTACT, MEVENT idx) override
	{
		return (idx >= 1) ? idx - 1 : 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// database link functions

static int pattern_makeDatabase(const wchar_t*)
{
	return 1;
}

static int pattern_grokHeader(const wchar_t *profile)
{
	return CDbxPattern().Open(profile);
}

static MDatabaseCommon* pattern_load(const wchar_t *profile, BOOL)
{
	std::auto_ptr<CDbxPattern> db(new CDbxPattern());
	if (db->Open(profile))
		return nullptr;

	db->Load();
	return db.release();
}

DATABASELINK g_patternDbLink =
{
	0,
	"pattern",
	L"Pattern-based file driver",
	pattern_makeDatabase,
	pattern_grokHeader,
	pattern_load
};
