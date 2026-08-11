#ifndef BOOTSTRAP_WIN32_H__
#define BOOTSTRAP_WIN32_H__

#include <string>
#include <windows.h>

// Called as the very first thing in main() of the Win32 build.
// Extracts embedded game data + runtime DLLs into a per-user temp cache
// and makes the DLLs findable before any delay-loaded import fires.
bool bootstrapSingleFile();

// Directory of the running executable (ANSI; always ends without '\\').
const std::string& getExeDir();

// Extracted data root ("...\\MCPE061-<hash>\\data"); empty if not a packed build.
const std::string& getDataRoot();

// Resolve an asset path (e.g. "images/terrain.png"):
//  1. <exeDir>/data/<file>   (portable data folder next to the exe)
//  2. <extracted cache>/<file>
//  3. ../../data/<file>      (source-tree dev fallback)
std::string resolveAssetPath(const std::string& filename);

// FPS-style mouse capture: hides the cursor, clips it to the window and
// recenters it each move so the game receives deltas (mouse look).
void win32SetMouseCapture(bool captured);

// Register the game window used for capture/recentering.
void win32SetWindowHandle(HWND hwnd);

// Called from WM_MOUSEMOVE. Feeds Mouse with either absolute coordinates
// (menus) or per-move deltas (captured, first-person look).
void win32HandleMouseMove(int xClient, int yClient);

// Called from WM_INPUT with raw hardware deltas (captured mode only).
void win32HandleRawMouse(int dx, int dy);

#endif /*BOOTSTRAP_WIN32_H__*/
