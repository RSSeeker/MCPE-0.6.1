#include "bootstrap_win32.h"

#include <windows.h>

#include "platform/input/Mouse.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Keep in sync with tools/pack_resources.py
#define IDR_DATAPACK  101
#define IDR_DLL_EGL   102
#define IDR_DLL_GLES  103
#define IDR_DLL_PNG   104
#define IDR_DLL_ZLIB  105
#define IDR_PACKHASH  106

namespace {

std::string g_exeDir;
std::string g_dataRoot;

// FPS mouse capture state (used by win32SetMouseCapture / win32HandleMouseMove)
HWND g_win32Hwnd = NULL;
bool g_win32Captured = false;
POINT g_win32LastPos = {0, 0};

std::string ansiFromWide(const std::wstring& w) {
	if (w.empty()) return std::string();
	int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
	std::string s((size_t)n, '\0');
	WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
	return s;
}

std::wstring wideFromAnsi(const std::string& a) {
	if (a.empty()) return std::wstring();
	int n = MultiByteToWideChar(CP_ACP, 0, a.c_str(), (int)a.size(), NULL, 0);
	std::wstring w((size_t)n, L'\0');
	MultiByteToWideChar(CP_ACP, 0, a.c_str(), (int)a.size(), &w[0], n);
	return w;
}

bool readResource(int resId, std::vector<unsigned char>& out) {
	HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(resId), RT_RCDATA);
	if (!hRes) return false;
	HGLOBAL hGlob = LoadResource(NULL, hRes);
	if (!hGlob) return false;
	DWORD size = SizeofResource(NULL, hRes);
	const unsigned char* data = (const unsigned char*)LockResource(hGlob);
	if (!data) return false;
	out.assign(data, data + size);
	return true;
}

bool writeFileAll(const std::wstring& path, const unsigned char* data, size_t size) {
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	BOOL ok = WriteFile(h, data, (DWORD)size, &written, NULL);
	CloseHandle(h);
	return ok && (size_t)written == size;
}

bool ensureDir(const std::wstring& dir) {
	if (dir.empty()) return false;
	// Walk the path, creating each component (skip the drive root).
	size_t start = (dir.size() >= 2 && dir[1] == L':') ? 3 : 0;
	for (size_t i = start; i <= dir.size(); ++i) {
		if (i == dir.size() || dir[i] == L'\\') {
			std::wstring part = dir.substr(0, i);
			if (part.empty()) continue;
			if (!CreateDirectoryW(part.c_str(), NULL) &&
				GetLastError() != ERROR_ALREADY_EXISTS)
				return false;
		}
	}
	return true;
}

bool fileExistsW(const std::wstring& path) {
	DWORD attr = GetFileAttributesW(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::string dirNameOf(const std::string& path) {
	size_t pos = path.find_last_of("\\/");
	if (pos == std::string::npos) return ".";
	if (pos == 0) return path.substr(0, 1);
	return path.substr(0, pos);
}

std::wstring wdirNameOf(const std::wstring& path) {
	size_t pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos) return L".";
	if (pos == 0) return path.substr(0, 1);
	return path.substr(0, pos);
}

// Parse the MCPEPACK1 archive and write its files below destRoot.
bool extractDataPack(const std::wstring& destRoot,
	const std::vector<unsigned char>& pack) {
	const unsigned char* p = pack.data();
	size_t total = pack.size();
	if (total < 12 || memcmp(p, "MCPEPACK", 8) != 0) return false;

	unsigned int count;
	memcpy(&count, p + 8, 4);
	p += 12;

	for (unsigned int i = 0; i < count; ++i) {
		if ((size_t)(p - pack.data()) + 4 > total) return false;
		unsigned int nameLen;
		memcpy(&nameLen, p, 4);
		p += 4;
		if ((size_t)(p - pack.data()) + nameLen + 4 > total) return false;

		std::string rel((const char*)p, nameLen);
		p += nameLen;

		unsigned int dataLen;
		memcpy(&dataLen, p, 4);
		p += 4;
		if ((size_t)(p - pack.data()) + dataLen > total) return false;

		// Sanity: only relative names without ".." are accepted.
		if (rel.empty() || rel.find("..") != std::string::npos ||
			rel[0] == '/' || rel[0] == '\\' || rel.find(':') != std::string::npos) {
			p += dataLen;
			continue;
		}

		std::wstring relW;
		for (size_t k = 0; k < rel.size(); ++k)
			relW += (wchar_t)(unsigned char)rel[k];
		for (size_t k = 0; k < relW.size(); ++k)
			if (relW[k] == L'/') relW[k] = L'\\';

		std::wstring full = destRoot + L"\\" + relW;
		if (!ensureDir(wdirNameOf(full))) {
			p += dataLen;
			continue;
		}
		if (!writeFileAll(full, p, dataLen)) return false;
		p += dataLen;
	}
	return true;
}

} // namespace

void win32SetMouseCapture(bool captured) {
	if (captured == g_win32Captured) return;
	g_win32Captured = captured;

	if (captured && g_win32Hwnd) {
		SetCapture(g_win32Hwnd);
		ShowCursor(FALSE);

		RECT rc;
		GetClientRect(g_win32Hwnd, &rc);
		POINT center;
		center.x = rc.right / 2;
		center.y = rc.bottom / 2;
		ClientToScreen(g_win32Hwnd, &center);
		g_win32LastPos = center;
		SetCursorPos(center.x, center.y);
	} else {
		ReleaseCapture();
		ClipCursor(NULL);
		while (ShowCursor(TRUE) < 0) {}
	}
}

void win32SetWindowHandle(HWND hwnd) {
	g_win32Hwnd = hwnd;
}

void win32HandleMouseMove(int xClient, int yClient) {
	if (g_win32Captured && g_win32Hwnd) {
		// Look deltas come from raw input (WM_INPUT); here we only keep the
		// cursor centered and the reported position current.
		Mouse::feed(MouseAction::ACTION_MOVE, 0, (short)xClient, (short)yClient, 0, 0);

		RECT rc;
		GetClientRect(g_win32Hwnd, &rc);
		POINT center;
		center.x = rc.right / 2;
		center.y = rc.bottom / 2;
		ClientToScreen(g_win32Hwnd, &center);
		g_win32LastPos = center;
		SetCursorPos(center.x, center.y);
	} else {
		Mouse::feed(MouseAction::ACTION_MOVE, 0, (short)xClient, (short)yClient);
	}
}

void win32HandleRawMouse(int dx, int dy) {
	if (!g_win32Captured) return;
	Mouse::feed(MouseAction::ACTION_MOVE, 0,
		(short)Mouse::getX(), (short)Mouse::getY(), (short)dx, (short)dy);
}

bool bootstrapSingleFile() {
	// 1. Locate the exe directory (ANSI, matching the game's narrow file APIs).
	char exePath[MAX_PATH] = {0};
	DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
	if (pathLen == 0 || pathLen >= MAX_PATH) return false;
	g_exeDir = dirNameOf(exePath);

	// 2. Read the pack hash resource; a non-packed (plain dev) build just skips.
	std::vector<unsigned char> hashRes;
	if (!readResource(IDR_PACKHASH, hashRes)) return true;
	std::string hashStr((const char*)hashRes.data(), hashRes.size());
	while (!hashStr.empty() &&
		(hashStr[hashStr.size() - 1] == '\r' || hashStr[hashStr.size() - 1] == '\n' ||
		 hashStr[hashStr.size() - 1] == ' '))
		hashStr.erase(hashStr.size() - 1);
	if (hashStr.empty()) return true;

	// 3. Per-user cache dir: %TEMP%\MCPE061-<hash>
	char tmpBuf[MAX_PATH] = {0};
	if (GetTempPathA(MAX_PATH, tmpBuf) == 0) return false;
	std::string cacheDir = std::string(tmpBuf) + "MCPE061-" + hashStr;
	std::wstring cacheDirW = wideFromAnsi(cacheDir);

	// 4. Skip extraction when the cache is already complete.
	std::wstring markerPath = cacheDirW + L"\\pack.ok";
	if (fileExistsW(markerPath)) {
		g_dataRoot = cacheDir + "\\data";
		SetDllDirectoryW(cacheDirW.c_str());
		return true;
	}

	if (!ensureDir(cacheDirW)) return false;

	// 5. Extract the runtime DLLs.
	static const int dllIds[] = { IDR_DLL_EGL, IDR_DLL_GLES, IDR_DLL_PNG, IDR_DLL_ZLIB };
	static const char* dllNames[] = {
		"libEGL.dll", "libgles_cm.dll", "libpng12.dll", "zlib1.dll"
	};
	for (int i = 0; i < 4; ++i) {
		std::vector<unsigned char> dll;
		if (!readResource(dllIds[i], dll)) return false;
		if (!writeFileAll(cacheDirW + L"\\" + wideFromAnsi(dllNames[i]),
			dll.data(), dll.size()))
			return false;
	}

	// 6. Extract the game data pack below cacheDir\data.
	std::vector<unsigned char> pack;
	if (!readResource(IDR_DATAPACK, pack)) return false;
	if (!extractDataPack(cacheDirW + L"\\data", pack)) return false;

	// 7. Write the completion marker and activate the DLL search path.
	if (!writeFileAll(markerPath, (const unsigned char*)hashStr.data(), hashStr.size()))
		return false;
	SetDllDirectoryW(cacheDirW.c_str());

	g_dataRoot = cacheDir + "\\data";
	return true;
}

const std::string& getExeDir() {
	return g_exeDir;
}

const std::string& getDataRoot() {
	return g_dataRoot;
}

std::string resolveAssetPath(const std::string& filename) {
	if (!g_exeDir.empty()) {
		std::string p = g_exeDir + "\\data\\" + filename;
		if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
	}
	if (!g_dataRoot.empty()) {
		std::string p = g_dataRoot + "\\" + filename;
		if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
	}
	return "../../data/" + filename;
}
