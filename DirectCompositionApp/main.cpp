#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <commdlg.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl.h>
#include "DCompHelper.h"
#include "TreeSitterHighlighter.h"
#include "DirectManipHelper.h"

using Microsoft::WRL::ComPtr;

static std::unique_ptr<DCompHelper> g_dcomp;
static ComPtr<DirectManipHelper> g_dmanip;
static TreeSitterHighlighter g_highlighter;
static std::wstring g_filePath;

static bool IsCppFile(const std::wstring& path) {
    if (path.empty()) return false;
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = path.substr(dot);
    for (auto& c : ext) {
        if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
    }
    return (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".cc" || ext == L".cxx" || ext == L".c");
}
static HWND g_hwnd = nullptr;

static std::wstring g_text;
static size_t g_cursorPos = 0;
static size_t g_selStart = 0;
static size_t g_selEnd = 0;
static float g_scrollY = 0;
static bool g_caretVisible = true;
static bool g_layoutDirty = true;
static bool g_dragging = false;
static int g_windowW = 800;
static int g_windowH = 600;
static float g_marginLeft = 48.0f;
static float g_marginTop = 62.0f;
static const float g_fontSize = 16.0f;
static const float g_menuBarH = 24.0f;

static ComPtr<IDWriteTextFormat> g_textFormat;
static ComPtr<IDWriteTextLayout> g_textLayout;

enum MenuAction {
    MA_NEW, MA_OPEN, MA_SAVE, MA_SAVEAS, MA_EXIT,
    MA_UNDO, MA_REDO, MA_CUT, MA_COPY, MA_PASTE, MA_DELETE, MA_SELECTALL, MA_TIMEDATE,
    MA_WORDWRAP, MA_FONT, MA_ABOUT, MA_NONE
};

struct MenuItem {
    const wchar_t* label;
    const wchar_t* shortcut;
    int action;
    bool separator;
};

struct MenuDef {
    const wchar_t* label;
    MenuItem items[16];
    int count;
    float x, w;
};

static MenuDef g_menus[] = {
    { L"File", {
        { L"New",           L"Ctrl+N",          MA_NEW },
        { L"Open...",       L"Ctrl+O",          MA_OPEN },
        { L"Save",          L"Ctrl+S",          MA_SAVE },
        { L"Save As...",    L"Ctrl+Shift+S",    MA_SAVEAS },
        { L"", L"", MA_NONE, true },
        { L"Exit",          L"Alt+F4",          MA_EXIT },
    }, 6, 0, 0 },
    { L"Edit", {
        { L"Undo",          L"Ctrl+Z",          MA_UNDO },
        { L"Redo",          L"Ctrl+Y",          MA_REDO },
        { L"", L"", MA_NONE, true },
        { L"Cut",           L"Ctrl+X",          MA_CUT },
        { L"Copy",          L"Ctrl+C",          MA_COPY },
        { L"Paste",         L"Ctrl+V",          MA_PASTE },
        { L"Delete",        L"Del",             MA_DELETE },
        { L"", L"", MA_NONE, true },
        { L"Select All",    L"Ctrl+A",          MA_SELECTALL },
        { L"Time/Date",     L"F5",              MA_TIMEDATE },
    }, 10, 0, 0 },
    { L"Format", {
        { L"Word Wrap",     L"",                MA_WORDWRAP },
        { L"Font...",       L"",                MA_FONT },
    }, 2, 0, 0 },
    { L"View", {
        { L"Zoom In",        L"Ctrl+=",          MA_NONE },
        { L"Zoom Out",       L"Ctrl+-",          MA_NONE },
        { L"", L"", MA_NONE, true },
        { L"Status Bar",     L"",                MA_NONE },
    }, 4, 0, 0 },
    { L"Help", {
        { L"About",         L"",                MA_ABOUT },
    }, 1, 0, 0 },
};
static const int g_menuCount = 5;

static int g_menuOpen = -1;
static int g_menuHover = -1;
static int g_dropHover = -1;
static bool g_menuTracking = false;

static bool g_ctxOpen = false;
static int g_ctxHover = -1;
static float g_ctxX = 0;
static float g_ctxY = 0;

struct EditAction { size_t pos; std::wstring removed; std::wstring inserted; };
static std::vector<EditAction> g_undoStack;
static std::vector<EditAction> g_redoStack;

static ComPtr<IDWriteTextFormat> g_menuFmt;

static HCURSOR g_lastCursor = nullptr;
static ComPtr<IDWriteTextFormat> g_lineNoFmt;
static bool g_wordWrap = true;

static void UpdateLayout();
static void EnsureCursorVisible();
static float GetTotalDocumentHeight();
static void RenderEditor();
static void HandleKey(WPARAM wParam);
static size_t CountNewlinesBefore(size_t pos);
static size_t LastNewlineBefore(size_t pos);

static int GetLineCount() {
    if (!g_textLayout) return 1;
    UINT32 lineCount = 0;
    g_textLayout->GetLineMetrics(nullptr, 0, &lineCount);
    return (int)lineCount;
}

static void UpdateGutterWidth() {
    int lines = GetLineCount();
    if (lines < 1) lines = 1;
    int digits = 1;
    int n = lines;
    while (n >= 10) { digits++; n /= 10; }
    float pad = 8.0f;
    g_marginLeft = pad + digits * 6.0f + 6.0f + pad;
}

static size_t ClampPos(size_t p) {
    if (p > g_text.length()) p = g_text.length();
    return p;
}

static void Invalidate() {
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void InitMenuBar() {
    float x = 2.0f;
    for (int i = 0; i < g_menuCount; i++) {
        g_menus[i].x = x;
        g_menus[i].w = (float)wcslen(g_menus[i].label) * 8.0f + 16.0f;
        x += g_menus[i].w;
    }
}

static const float g_statusBarH = 36.0f;

static int HitTestMenuBar(float mx, float my) {
    if (my >= g_statusBarH && my < g_statusBarH + g_menuBarH) {
        for (int i = 0; i < g_menuCount; i++) {
            if (mx >= g_menus[i].x && mx < g_menus[i].x + g_menus[i].w)
                return i;
        }
    }
    return -1;
}

static int HitTestDropdown(float mx, float my) {
    if (g_menuOpen < 0) return -1;
    MenuDef& m = g_menus[g_menuOpen];
    float dw = 240.0f;
    float dx = m.x;
    float dy = g_statusBarH + g_menuBarH;
    if (mx < dx || mx > dx + dw || my < dy) return -1;
    float iy = dy;
    for (int i = 0; i < m.count; i++) {
        float ih = m.items[i].separator ? 6.0f : 24.0f;
        if (my >= iy && my < iy + ih) {
            if (!m.items[i].separator) return i;
            return -1;
        }
        iy += ih;
    }
    return -1;
}

static void CloseMenu() {
    g_menuOpen = -1;
    g_dropHover = -1;
    g_menuTracking = false;
}

struct CtxMenuItem { const wchar_t* label; int action; bool separator; };
static CtxMenuItem g_ctxItems[] = {
    { L"Cut",         MA_CUT },
    { L"Copy",        MA_COPY },
    { L"Paste",       MA_PASTE },
    { L"", MA_NONE, true },
    { L"Select All",  MA_SELECTALL },
};
static const int g_ctxItemCount = 5;

static void CloseCtxMenu() {
    g_ctxOpen = false;
    g_ctxHover = -1;
}

static int HitTestCtxMenu(float mx, float my) {
    if (!g_ctxOpen) return -1;
    float dw = 200.0f;
    float dh = 24.0f;
    if (mx < g_ctxX || mx > g_ctxX + dw || my < g_ctxY) return -1;
    float iy = g_ctxY;
    for (int i = 0; i < g_ctxItemCount; i++) {
        float ih = g_ctxItems[i].separator ? 6.0f : dh;
        if (my >= iy && my < iy + ih) {
            if (!g_ctxItems[i].separator) return i;
            return -1;
        }
        iy += ih;
    }
    return -1;
}

static void PushUndo(size_t pos, const std::wstring& del, const std::wstring& ins) {
    g_undoStack.push_back({ pos, del, ins });
    g_redoStack.clear();
}

static void DoNew() {
    if (!g_text.empty()) PushUndo(0, g_text, L"");
    g_text.clear();
    g_cursorPos = g_selStart = g_selEnd = 0;
    g_scrollY = 0;
    g_filePath.clear();
}

static void DoUndo() {
    if (g_undoStack.empty()) return;
    EditAction a = std::move(g_undoStack.back());
    g_undoStack.pop_back();
    g_text.erase(a.pos, a.inserted.length());
    g_text.insert(a.pos, a.removed);
    g_cursorPos = a.pos + a.removed.length();
    g_selStart = g_selEnd = g_cursorPos;
    g_redoStack.push_back(std::move(a));
}

static void DoRedo() {
    if (g_redoStack.empty()) return;
    EditAction a = std::move(g_redoStack.back());
    g_redoStack.pop_back();
    g_text.erase(a.pos, a.removed.length());
    g_text.insert(a.pos, a.inserted);
    g_cursorPos = a.pos + a.inserted.length();
    g_selStart = g_selEnd = g_cursorPos;
    g_undoStack.push_back(std::move(a));
}

static void DoOpen() {
    wchar_t file[MAX_PATH] = {};
    if (!g_filePath.empty()) wcscpy_s(file, g_filePath.c_str());
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner = g_hwnd;
    of.lpstrFilter = L"C++ Files (*.cpp;*.h;*.hpp;*.cc)\0*.cpp;*.h;*.hpp;*.cc\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Open";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    BOOL ok = GetOpenFileNameW(&of);
    if (!ok) {
        DWORD err = CommDlgExtendedError();
        if (err) {
            wchar_t msg[128];
            swprintf_s(msg, L"[Open] GetOpenFileNameW failed: CommDlgError=0x%04X\n", err);
            OutputDebugStringW(msg);
        }
        return;
    }
    HANDLE hFile = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, nullptr);
        if (size != INVALID_FILE_SIZE && size > 0) {
            std::vector<char> buf(size);
            DWORD read;
            ReadFile(hFile, buf.data(), size, &read, nullptr);
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.data(), size, nullptr, 0);
            g_text.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, buf.data(), size, g_text.data(), wlen);
        } else {
            g_text.clear();
        }
        CloseHandle(hFile);
        g_filePath = file;
        g_cursorPos = g_selStart = g_selEnd = 0;
        g_scrollY = 0;
        g_undoStack.clear();
        g_redoStack.clear();
    }
}

static void DoSave() {
    wchar_t file[MAX_PATH] = {};
    if (!g_filePath.empty()) wcscpy_s(file, g_filePath.c_str());
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner = g_hwnd;
    of.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrDefExt = L"txt";
    of.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&of)) {
        HANDLE hFile = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            int len = WideCharToMultiByte(CP_UTF8, 0, g_text.c_str(), (int)g_text.length(), nullptr, 0, nullptr, nullptr);
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_UTF8, 0, g_text.c_str(), (int)g_text.length(), buf.data(), len, nullptr, nullptr);
            DWORD written;
            WriteFile(hFile, buf.data(), len, &written, nullptr);
            CloseHandle(hFile);
            g_filePath = file;
        }
    }
}

static void ExecMenuAction(int action) {
    switch (action) {
    case MA_NEW:    DoNew(); break;
    case MA_OPEN:   DoOpen(); break;
    case MA_SAVE:   DoSave(); break;
    case MA_SAVEAS: {
        std::wstring old = g_filePath;
        g_filePath.clear();
        DoSave();
        if (g_filePath.empty()) g_filePath = old;
        break;
    }
    case MA_EXIT:
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        break;
    case MA_UNDO:   DoUndo(); break;
    case MA_REDO:   DoRedo(); break;
    case MA_CUT:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            std::wstring clip = g_text.substr(a, b - a);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (clip.size() + 1) * sizeof(wchar_t));
            if (hMem) { wchar_t* p = (wchar_t*)GlobalLock(hMem); wcscpy_s(p, clip.size() + 1, clip.c_str()); GlobalUnlock(hMem); OpenClipboard(g_hwnd); EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, hMem); CloseClipboard(); }
            PushUndo(a, clip, L"");
            g_text.erase(a, b - a);
            g_cursorPos = a;
            g_selStart = g_selEnd = g_cursorPos;
        }
        break;
    case MA_COPY:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            std::wstring clip = g_text.substr(a, b - a);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (clip.size() + 1) * sizeof(wchar_t));
            if (hMem) { wchar_t* p = (wchar_t*)GlobalLock(hMem); wcscpy_s(p, clip.size() + 1, clip.c_str()); GlobalUnlock(hMem); OpenClipboard(g_hwnd); EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, hMem); CloseClipboard(); }
        }
        break;
    case MA_PASTE: {
        OpenClipboard(g_hwnd);
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t* p = (const wchar_t*)GlobalLock(hData);
            if (p) {
                std::wstring ins(p);
                if (g_selStart != g_selEnd) {
                    size_t a = (std::min)(g_selStart, g_selEnd);
                    size_t b = (std::max)(g_selStart, g_selEnd);
                    PushUndo(a, g_text.substr(a, b - a), ins);
                    g_text.erase(a, b - a);
                    g_cursorPos = a;
                } else {
                    PushUndo(g_cursorPos, L"", ins);
                }
                g_text.insert(g_cursorPos, ins);
                g_cursorPos += ins.length();
                g_selStart = g_selEnd = g_cursorPos;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
        break;
    }
    case MA_DELETE:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            PushUndo(a, g_text.substr(a, b - a), L"");
            g_text.erase(a, b - a);
            g_cursorPos = a;
        } else if (g_cursorPos < g_text.length()) {
            PushUndo(g_cursorPos, g_text.substr(g_cursorPos, 1), L"");
            g_text.erase(g_cursorPos, 1);
        }
        g_selStart = g_selEnd = g_cursorPos;
        break;
    case MA_SELECTALL:
        g_selStart = 0;
        g_selEnd = g_text.length();
        g_cursorPos = g_selEnd;
        break;
    case MA_TIMEDATE: {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[64];
        _snwprintf_s(buf, 64, L"%d/%d/%d %d:%02d:%02d",
            st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
        std::wstring ins(buf);
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            PushUndo(a, g_text.substr(a, b - a), ins);
            g_text.erase(a, b - a);
            g_cursorPos = a;
        } else {
            PushUndo(g_cursorPos, L"", ins);
        }
        g_text.insert(g_cursorPos, ins);
        g_cursorPos += ins.length();
        g_selStart = g_selEnd = g_cursorPos;
        break;
    }
    case MA_WORDWRAP:
        g_wordWrap = !g_wordWrap;
        break;
    case MA_FONT:
    case MA_ABOUT:
        break;
    }
    g_layoutDirty = true;
    EnsureCursorVisible();
    Invalidate();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_dcomp = std::make_unique<DCompHelper>();
        if (!g_dcomp->Initialize(hwnd)) {
            OutputDebugStringA("[Editor] DComp init failed\n");
        }

        g_dmanip = new DirectManipHelper();
        if (g_dmanip->Initialize(hwnd)) {
            OutputDebugStringA("[Editor] DirectManip initialized\n");
        } else {
            g_dmanip.Reset();
        }

        if (!g_highlighter.Initialize()) {
            OutputDebugStringA("[Editor] Tree-sitter highlighter init failed\n");
        }

        g_dcomp->GetDWriteFactory()->CreateTextFormat(
            L"Cascadia Mono", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, g_fontSize, L"en-us",
            g_textFormat.GetAddressOf());

        if (!g_textFormat) {
            g_dcomp->GetDWriteFactory()->CreateTextFormat(
                L"Consolas", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, g_fontSize, L"en-us",
                g_textFormat.GetAddressOf());
        }

        g_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        g_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        g_dcomp->GetDWriteFactory()->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
            g_menuFmt.GetAddressOf());
        if (g_menuFmt) {
            g_menuFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            g_menuFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        g_dcomp->GetDWriteFactory()->CreateTextFormat(
            L"Cascadia Mono", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
            g_lineNoFmt.GetAddressOf());
        if (!g_lineNoFmt) {
            g_dcomp->GetDWriteFactory()->CreateTextFormat(
                L"Consolas", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
                g_lineNoFmt.GetAddressOf());
        }
        if (g_lineNoFmt) {
            g_lineNoFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            g_lineNoFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        InitMenuBar();
        SetTimer(hwnd, 1, 500, nullptr);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        g_marginTop = 62.0f;
        if (g_dmanip) {
            g_dmanip->Update();
            g_scrollY = g_dmanip->GetScrollY();
        }
        g_dcomp->UpdateFrameStatistics();
        RenderEditor();

        if (g_dmanip && g_dmanip->IsAnimating()) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CHAR:
        if (wParam == 27) {
            if (g_ctxOpen) {
                CloseCtxMenu();
                Invalidate();
            }
            return 0;
        }
        if (wParam == 1) {
            g_selStart = 0;
            g_selEnd = g_text.length();
            g_cursorPos = g_selEnd;
            Invalidate();
            return 0;
        }
        if (wParam == 3 || wParam == 24 || wParam == 22 || wParam == 26) {
            if (wParam == 3 || wParam == 24) {
                if (g_selStart != g_selEnd) {
                    size_t a = (std::min)(g_selStart, g_selEnd);
                    size_t b = (std::max)(g_selStart, g_selEnd);
                    std::wstring clip = g_text.substr(a, b - a);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (clip.size() + 1) * sizeof(wchar_t));
                    if (hMem) {
                        wchar_t* p = (wchar_t*)GlobalLock(hMem);
                        wcscpy_s(p, clip.size() + 1, clip.c_str());
                        GlobalUnlock(hMem);
                        OpenClipboard(hwnd);
                        EmptyClipboard();
                        SetClipboardData(CF_UNICODETEXT, hMem);
                        CloseClipboard();
                    }
                }
            }
            if (wParam == 24 && g_selStart != g_selEnd) {
                size_t a = (std::min)(g_selStart, g_selEnd);
                size_t b = (std::max)(g_selStart, g_selEnd);
                g_text.erase(a, b - a);
                g_cursorPos = a;
                g_selStart = g_selEnd = g_cursorPos;
            }
            if (wParam == 22) {
                OpenClipboard(hwnd);
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    const wchar_t* p = (const wchar_t*)GlobalLock(hData);
                    if (p) {
                        if (g_selStart != g_selEnd) {
                            size_t a = (std::min)(g_selStart, g_selEnd);
                            size_t b = (std::max)(g_selStart, g_selEnd);
                            g_text.erase(a, b - a);
                            g_cursorPos = a;
                        }
                        g_text.insert(g_cursorPos, p);
                        g_cursorPos += wcslen(p);
                        g_selStart = g_selEnd = g_cursorPos;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            if (wParam == 26) {
                g_cursorPos = g_selStart = g_selEnd;
            }
            g_layoutDirty = true;
            EnsureCursorVisible();
            Invalidate();
            return 0;
        }
        if (wParam >= 32) {
            if (g_selStart != g_selEnd) {
                size_t a = (std::min)(g_selStart, g_selEnd);
                size_t b = (std::max)(g_selStart, g_selEnd);
                g_text.erase(a, b - a);
                g_cursorPos = a;
            }
            g_text.insert(g_cursorPos, 1, (wchar_t)wParam);
            g_cursorPos++;
            g_selStart = g_selEnd = g_cursorPos;
            g_caretVisible = true;
            g_layoutDirty = true;
            EnsureCursorVisible();
            Invalidate();
        }
        return 0;

    case WM_KEYDOWN:
        HandleKey(wParam);
        return 0;

    case WM_LBUTTONDOWN: {
        float mx = (float)GET_X_LPARAM(lParam);
        float my = (float)GET_Y_LPARAM(lParam);

        if (g_ctxOpen) {
            int item = HitTestCtxMenu(mx, my);
            if (item >= 0) {
                ExecMenuAction(g_ctxItems[item].action);
            }
            CloseCtxMenu();
            Invalidate();
            return 0;
        }

        if (g_menuOpen >= 0) {
            int item = HitTestDropdown(mx, my);
            if (item >= 0) {
                MenuDef& m = g_menus[g_menuOpen];
                ExecMenuAction(m.items[item].action);
            }
            CloseMenu();
            Invalidate();
            return 0;
        }

        int menu = HitTestMenuBar(mx, my);
        if (menu >= 0) {
            CloseCtxMenu();
            g_menuOpen = menu;
            g_dropHover = -1;
            g_menuTracking = true;
            Invalidate();
            return 0;
        }

        CloseMenu();
        SetFocus(hwnd);

        if (g_textLayout && mx >= g_marginLeft) {
            float textX = mx - g_marginLeft;
            float textY = my - g_marginTop + g_scrollY;
            BOOL isTrailing, isInside;
            UINT32 pos = 0;
            DWRITE_HIT_TEST_METRICS hit;
            if (SUCCEEDED(g_textLayout->HitTestPoint(textX, textY, &isTrailing, &isInside, &hit))) {
                pos = hit.textPosition;
                if (isTrailing && pos < g_text.length()) pos++;
            }

            if (wParam & MK_SHIFT) {
                g_selEnd = pos;
            } else {
                g_selStart = pos;
                g_selEnd = pos;
            }
            g_cursorPos = pos;
            g_caretVisible = true;
            g_dragging = true;
            SetCapture(hwnd);
            Invalidate();
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_dragging) {
            g_dragging = false;
            ReleaseCapture();
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        float mx = (float)GET_X_LPARAM(lParam);
        float my = (float)GET_Y_LPARAM(lParam);

        if (g_menuOpen >= 0) return 0;

        int menu = HitTestMenuBar(mx, my);
        if (menu >= 0) return 0;

        if (g_ctxOpen) {
            int item = HitTestCtxMenu(mx, my);
            if (item >= 0) {
                ExecMenuAction(g_ctxItems[item].action);
            }
            CloseCtxMenu();
            Invalidate();
            return 0;
        }

        if (g_textLayout && mx >= g_marginLeft && my >= g_marginTop) {
            float textX = mx - g_marginLeft;
            float textY = my - g_marginTop + g_scrollY;
            BOOL isTrailing, isInside;
            UINT32 pos = 0;
            DWRITE_HIT_TEST_METRICS hit;
            if (SUCCEEDED(g_textLayout->HitTestPoint(textX, textY, &isTrailing, &isInside, &hit))) {
                pos = hit.textPosition;
                if (isTrailing && pos < g_text.length()) pos++;
            }
            if (g_selStart == g_selEnd) {
                g_selStart = pos;
                g_selEnd = pos;
                g_cursorPos = pos;
            }

            float dw = 200.0f;
            float dh = 0;
            for (int i = 0; i < g_ctxItemCount; i++)
                dh += g_ctxItems[i].separator ? 6.0f : 24.0f;

            float cx = mx;
            float cy = my;
            if (cx + dw > (float)g_windowW) cx = (float)g_windowW - dw - 4;
            if (cy + dh > (float)g_windowH) cy = (float)g_windowH - dh - 4;

            g_ctxX = cx;
            g_ctxY = cy;
            g_ctxOpen = true;
            g_ctxHover = -1;
            Invalidate();
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        float mx = (float)GET_X_LPARAM(lParam);
        float my = (float)GET_Y_LPARAM(lParam);

        if (g_ctxOpen) {
            int item = HitTestCtxMenu(mx, my);
            if (item != g_ctxHover) {
                g_ctxHover = item;
                Invalidate();
            }
            return 0;
        }

        if (g_menuTracking) {
            int menu = HitTestMenuBar(mx, my);
            if (menu >= 0 && menu != g_menuOpen) {
                g_menuOpen = menu;
                g_dropHover = -1;
            }
            int item = HitTestDropdown(mx, my);
            if (item != g_dropHover) {
                g_dropHover = item;
            }
            Invalidate();
            return 0;
        }

        if (g_dragging && g_textLayout && mx >= g_marginLeft) {
            float textX = mx - g_marginLeft;
            float textY = my - g_marginTop + g_scrollY;
            BOOL isTrailing, isInside;
            UINT32 pos = 0;
            DWRITE_HIT_TEST_METRICS hit;
            if (SUCCEEDED(g_textLayout->HitTestPoint(textX, textY, &isTrailing, &isInside, &hit))) {
                pos = hit.textPosition;
                if (isTrailing && pos < g_text.length()) pos++;
            }
            g_selEnd = pos;
            g_cursorPos = pos;
            g_caretVisible = true;
            Invalidate();
            return 0;
        }

        bool inMenuBar = (my >= g_statusBarH && my < g_statusBarH + g_menuBarH);
        bool inStatusBar = (my < g_statusBarH);
        bool inHintBar = (my >= g_windowH - 20);
        int menu = HitTestMenuBar(mx, my);
        if (menu != g_menuHover) {
            g_menuHover = menu;
            Invalidate();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float visibleH = (float)g_windowH - g_marginTop - 24.0f;
        float totalH = GetTotalDocumentHeight();
        float currentScroll = g_dmanip ? g_dmanip->GetScrollY() : g_scrollY;
        float newScroll = currentScroll - (float)delta * 0.5f;
        if (totalH > 0 && newScroll > totalH - visibleH) newScroll = totalH - visibleH;
        if (newScroll < 0) newScroll = 0;
        if (g_dmanip) {
            g_dmanip->SetContentHeight(totalH);
            g_dmanip->ScrollTo(newScroll, FALSE);
        }
        return 0;
    }

    case WM_KILLFOCUS:
        if (g_menuOpen >= 0) {
            CloseMenu();
            Invalidate();
        }
        return 0;

    case WM_SETCURSOR: {
        if (LOWORD(lParam) != HTCLIENT) {
            g_lastCursor = nullptr;
            return DefWindowProcW(hwnd, WM_SETCURSOR, wParam, lParam);
        }

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);

        LPCWSTR cursorId = IDC_IBEAM;

        if (g_dragging) {
            cursorId = IDC_IBEAM;
        } else if (g_ctxOpen) {
            int item = HitTestCtxMenu((float)pt.x, (float)pt.y);
            cursorId = item >= 0 ? IDC_HAND : IDC_ARROW;
        } else {
            bool overDropdown = false;
            if (g_menuOpen >= 0) {
                int item = HitTestDropdown((float)pt.x, (float)pt.y);
                overDropdown = (item >= 0);
            }

            bool inMenuBar = (pt.y >= g_statusBarH && pt.y < g_statusBarH + g_menuBarH);
            bool inStatusBar = (pt.y < g_statusBarH);
            bool inHintBar = (pt.y >= g_windowH - 20);
            bool inGutter = (pt.x < g_marginLeft);

            if (inMenuBar || overDropdown) {
                cursorId = IDC_HAND;
            } else if (inStatusBar || inHintBar || inGutter) {
                cursorId = IDC_ARROW;
            } else {
                cursorId = IDC_IBEAM;
            }
        }

        HCURSOR loaded = LoadCursor(nullptr, cursorId);
        if (loaded != g_lastCursor) {
            g_lastCursor = loaded;
            SetCursor(loaded);
        }
        return 1;
    }

    case WM_TIMER:
        if (wParam == 1) {
            g_caretVisible = !g_caretVisible;
            Invalidate();
        } else if (wParam == 3) {
            KillTimer(hwnd, 3);
            g_dcomp->TryFinishResize(hwnd);
            Invalidate();
        }
        return 0;

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w > 0 && h > 0) {
            g_windowW = w;
            g_windowH = h;
            g_layoutDirty = true;
            g_dcomp->ResizeSwapChain(w, h);
            if (g_dmanip) {
                float visibleH = (float)h - 62.0f - 24.0f;
                if (visibleH < 1) visibleH = 1;
                g_dmanip->SetViewportSize((float)w, visibleH);
            }
            EnsureCursorVisible();
            Invalidate();
        }
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 300;
        RECT workArea = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        mmi->ptMaxPosition.x = workArea.left;
        mmi->ptMaxPosition.y = workArea.top;
        mmi->ptMaxSize.x = workArea.right - workArea.left;
        mmi->ptMaxSize.y = workArea.bottom - workArea.top;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_dcomp.reset();
        g_dmanip.Reset();
        PostQuitMessage(0);
        return 0;

    case WM_POINTERDOWN: {
        UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
        if (g_dmanip) {
            g_dmanip->AddContact(pointerId);
            BOOL handled = FALSE;
            MSG msg = { hwnd, uMsg, wParam, lParam };
            g_dmanip->ProcessInput(&msg, &handled);
            if (handled) {
                g_dmanip->Update();
                return 0;
            }
        }
        break;
    }

    default:
        if (g_dmanip) {
            BOOL handled = FALSE;
            MSG msg = { hwnd, uMsg, wParam, lParam };
            g_dmanip->ProcessInput(&msg, &handled);
            if (handled) {
                g_dmanip->Update();
                return 0;
            }
        }
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static size_t CountNewlinesBefore(size_t pos) {
    size_t count = 0;
    for (size_t i = 0; i < pos && i < g_text.length(); i++) {
        if (g_text[i] == L'\n') count++;
    }
    return count;
}

static size_t LastNewlineBefore(size_t pos) {
    if (pos == 0) return 0;
    for (size_t i = pos; i > 0; i--) {
        if (g_text[i - 1] == L'\n') return i;
    }
    return 0;
}

static void HandleKey(WPARAM wParam) {
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (ctrl) {
    switch (wParam) {
    case VK_F5:
        ExecMenuAction(MA_TIMEDATE);
        return;
        case 'C':
        case 'X': {
            if (g_selStart != g_selEnd) {
                size_t a = (std::min)(g_selStart, g_selEnd);
                size_t b = (std::max)(g_selStart, g_selEnd);
                std::wstring clip = g_text.substr(a, b - a);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (clip.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* p = (wchar_t*)GlobalLock(hMem);
                    wcscpy_s(p, clip.size() + 1, clip.c_str());
                    GlobalUnlock(hMem);
                    OpenClipboard(g_hwnd);
                    EmptyClipboard();
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    CloseClipboard();
                }
                if (wParam == 'X') {
                    g_text.erase(a, b - a);
                    g_cursorPos = a;
                    g_selStart = g_selEnd = g_cursorPos;
                }
            }
            EnsureCursorVisible();
            Invalidate();
            return;
        }
        case 'V': {
            OpenClipboard(g_hwnd);
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                const wchar_t* p = (const wchar_t*)GlobalLock(hData);
                if (p) {
                    if (g_selStart != g_selEnd) {
                        size_t a = (std::min)(g_selStart, g_selEnd);
                        size_t b = (std::max)(g_selStart, g_selEnd);
                        g_text.erase(a, b - a);
                        g_cursorPos = a;
                    }
                    g_text.insert(g_cursorPos, p);
                    g_cursorPos += wcslen(p);
                    g_selStart = g_selEnd = g_cursorPos;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            EnsureCursorVisible();
            Invalidate();
            return;
        }
        case 'A':
            g_selStart = 0;
            g_selEnd = g_text.length();
            g_cursorPos = g_selEnd;
            Invalidate();
            return;
        case 'N':
            DoNew();
            EnsureCursorVisible();
            Invalidate();
            return;
        case 'O':
            DoOpen();
            EnsureCursorVisible();
            Invalidate();
            return;
        case 'S':
            DoSave();
            Invalidate();
            return;
        case 'Z':
            DoUndo();
            EnsureCursorVisible();
            Invalidate();
            return;
        case 'Y':
            DoRedo();
            EnsureCursorVisible();
            Invalidate();
            return;
        case VK_HOME:
            g_cursorPos = g_selStart = g_selEnd = 0;
            EnsureCursorVisible();
            Invalidate();
            return;
        case VK_END:
            g_cursorPos = g_selStart = g_selEnd = g_text.length();
            EnsureCursorVisible();
            Invalidate();
            return;
        }
        return;
    }

    switch (wParam) {
    case VK_LEFT:
        if (g_cursorPos > 0) {
            g_cursorPos--;
            if (!shift) g_selStart = g_selEnd = g_cursorPos;
            else g_selEnd = g_cursorPos;
        }
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_RIGHT:
        if (g_cursorPos < g_text.length()) {
            g_cursorPos++;
            if (!shift) g_selStart = g_selEnd = g_cursorPos;
            else g_selEnd = g_cursorPos;
        }
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_UP: {
        UpdateLayout();
        if (!g_textLayout) break;
        FLOAT px, py;
        DWRITE_HIT_TEST_METRICS hit;
        if (SUCCEEDED(g_textLayout->HitTestTextPosition((UINT32)g_cursorPos, FALSE, &px, &py, &hit))) {
            float targetY = py - g_fontSize * 1.2f;
            if (targetY < 0) targetY = 0;
            BOOL isTrailing, isInside;
            DWRITE_HIT_TEST_METRICS hit2;
            g_textLayout->HitTestPoint(px + 0.5f, targetY, &isTrailing, &isInside, &hit2);
            g_cursorPos = ClampPos(hit2.textPosition);
            if (isTrailing) g_cursorPos = ClampPos(hit2.textPosition + 1);
            if (!shift) g_selStart = g_selEnd = g_cursorPos;
            else g_selEnd = g_cursorPos;
        }
        EnsureCursorVisible();
        Invalidate();
        break;
    }
    case VK_DOWN: {
        UpdateLayout();
        if (!g_textLayout) break;
        FLOAT px, py;
        DWRITE_HIT_TEST_METRICS hit;
        if (SUCCEEDED(g_textLayout->HitTestTextPosition((UINT32)g_cursorPos, FALSE, &px, &py, &hit))) {
            float targetY = py + hit.height + g_fontSize * 0.2f;
            BOOL isTrailing, isInside;
            DWRITE_HIT_TEST_METRICS hit2;
            g_textLayout->HitTestPoint(px + 0.5f, targetY, &isTrailing, &isInside, &hit2);
            g_cursorPos = ClampPos(hit2.textPosition);
            if (isTrailing) g_cursorPos = ClampPos(hit2.textPosition + 1);
            if (!shift) g_selStart = g_selEnd = g_cursorPos;
            else g_selEnd = g_cursorPos;
        }
        EnsureCursorVisible();
        Invalidate();
        break;
    }
    case VK_HOME:
        g_cursorPos = LastNewlineBefore(g_cursorPos);
        if (!shift) g_selStart = g_selEnd = g_cursorPos;
        else g_selEnd = g_cursorPos;
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_END: {
        size_t nl = g_text.find(L'\n', g_cursorPos);
        g_cursorPos = (nl != std::wstring::npos) ? nl : g_text.length();
        if (!shift) g_selStart = g_selEnd = g_cursorPos;
        else g_selEnd = g_cursorPos;
        EnsureCursorVisible();
        Invalidate();
        break;
    }
    case VK_BACK:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            g_text.erase(a, b - a);
            g_cursorPos = a;
        } else if (g_cursorPos > 0) {
            g_text.erase(g_cursorPos - 1, 1);
            g_cursorPos--;
        }
        g_selStart = g_selEnd = g_cursorPos;
        g_caretVisible = true;
        g_layoutDirty = true;
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_DELETE:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            g_text.erase(a, b - a);
            g_cursorPos = a;
        } else if (g_cursorPos < g_text.length()) {
            g_text.erase(g_cursorPos, 1);
        }
        g_selStart = g_selEnd = g_cursorPos;
        g_caretVisible = true;
        g_layoutDirty = true;
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_RETURN:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            g_text.erase(a, b - a);
            g_cursorPos = a;
        }
        g_text.insert(g_cursorPos, 1, L'\n');
        g_cursorPos++;
        g_selStart = g_selEnd = g_cursorPos;
        g_caretVisible = true;
        g_layoutDirty = true;
        EnsureCursorVisible();
        Invalidate();
        break;
    case VK_TAB:
        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);
            g_text.erase(a, b - a);
            g_cursorPos = a;
        }
        g_text.insert(g_cursorPos, 4, L' ');
        g_cursorPos += 4;
        g_selStart = g_selEnd = g_cursorPos;
        g_caretVisible = true;
        g_layoutDirty = true;
        EnsureCursorVisible();
        Invalidate();
        break;
    }
    g_caretVisible = true;
}

static std::string WToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(), &result[0], len, nullptr, nullptr);
    return result;
}

static void UpdateLayout() {
    if (!g_layoutDirty && g_textLayout) return;
    if (!g_dcomp) return;
    float maxW = (float)g_windowW - g_marginLeft * 2;
    float maxH = (float)g_windowH - g_marginTop - 24.0f;
    if (maxW < 1) maxW = 1;
    if (maxH < 1) maxH = 1;

    const wchar_t* text = g_text.c_str();
    UINT32 len = (UINT32)g_text.length();

    g_dcomp->GetDWriteFactory()->CreateTextLayout(
        text, len, g_textFormat.Get(),
        maxW, maxH, g_textLayout.GetAddressOf());

    if (g_textLayout) {
        g_textLayout->SetWordWrapping(g_wordWrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    if (IsCppFile(g_filePath)) {
        std::string utf8 = WToUtf8(g_text);
        g_highlighter.UpdateSource(utf8.c_str(), (uint32_t)utf8.size());
    } else {
        g_highlighter.UpdateSource("", 0);
    }

    if (g_dmanip && g_textLayout) {
        float totalH = GetTotalDocumentHeight();
        g_dmanip->SetContentHeight(totalH);
    }

    g_layoutDirty = false;
}

static float GetTotalDocumentHeight() {
    float totalH = 0;
    if (g_textLayout) {
        UINT32 lc = 0;
        g_textLayout->GetLineMetrics(nullptr, 0, &lc);
        if (lc > 0) {
            std::vector<DWRITE_LINE_METRICS> lm(lc);
            g_textLayout->GetLineMetrics(lm.data(), lc, &lc);
            for (UINT32 i = 0; i < lc; i++) {
                totalH += lm[i].height;
            }
        }
    }
    return totalH;
}

static void EnsureCursorVisible() {
    UpdateLayout();
    if (!g_textLayout) return;

    DWRITE_HIT_TEST_METRICS hit;
    FLOAT px, py;
    if (FAILED(g_textLayout->HitTestTextPosition((UINT32)g_cursorPos, FALSE, &px, &py, &hit)))
        return;

    float visibleH = (float)g_windowH - g_marginTop - 24.0f;
    float lineH = hit.height > 0 ? hit.height : g_fontSize * 1.15f;

    float currentScroll = g_dmanip ? g_dmanip->GetScrollY() : g_scrollY;
    float newScrollY = currentScroll;
    if (py < newScrollY) {
        newScrollY = py;
    } else if (py + lineH > newScrollY + visibleH) {
        newScrollY = py + lineH - visibleH;
    }
    if (newScrollY < 0) newScrollY = 0;

    if (fabsf(newScrollY - currentScroll) > 0.5f) {
        if (g_dmanip) {
            float totalH = GetTotalDocumentHeight();
            g_dmanip->SetContentHeight(totalH);
            g_dmanip->ScrollTo(newScrollY, FALSE);
        } else {
            g_scrollY = newScrollY;
            Invalidate();
        }
    }
}

static void RenderEditor() {
    if (!g_dcomp || !g_dcomp->GetSwapChain()) return;

    ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_dcomp->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return;

    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = g_dcomp->GetD2DContext()->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bmpProps, targetBitmap.GetAddressOf());
    if (FAILED(hr)) return;

    auto ctx = g_dcomp->GetD2DContext();
    ctx->SetTarget(targetBitmap.Get());
    ctx->BeginDraw();

    UpdateGutterWidth();

    ctx->Clear(D2D1::ColorF(0.10f, 0.10f, 0.12f));

    float hintBarH = 20.0f;

    UpdateLayout();

    if (g_textLayout && IsCppFile(g_filePath) && g_highlighter.GetHighlights().size() > 0) {
        auto ctx2 = g_dcomp->GetD2DContext();
        const std::string& utf8 = WToUtf8(g_text);

        std::vector<uint32_t> utf8ToUtf16;
        utf8ToUtf16.reserve(utf8.size() + 1);
        uint32_t utf16Idx = 0;
        for (size_t i = 0; i < g_text.length(); i++) {
            wchar_t wc = g_text[i];
            int bytes = 0;
            if (wc < 0x80) {
                bytes = 1;
            } else if (wc < 0x800) {
                bytes = 2;
            } else if (wc >= 0xD800 && wc <= 0xDBFF) {
                bytes = 4;
                i++;
            } else {
                bytes = 3;
            }
            for (int b = 0; b < bytes; b++) {
                utf8ToUtf16.push_back(utf16Idx);
            }
            utf16Idx += (bytes == 4) ? 2 : 1;
        }
        utf8ToUtf16.push_back(utf16Idx);

        for (auto& hl : g_highlighter.GetHighlights()) {
            if (hl.startByte >= utf8ToUtf16.size() || hl.endByte > utf8ToUtf16.size()) continue;
            uint32_t startChar = utf8ToUtf16[hl.startByte];
            uint32_t endChar = utf8ToUtf16[hl.endByte];
            if (startChar >= endChar) continue;

            D2D1_COLOR_F color;
            const char* name = hl.captureName;
            if (strstr(name, "keyword")) {
                color = D2D1::ColorF(0.56f, 0.78f, 0.96f);
            } else if (strstr(name, "function")) {
                color = D2D1::ColorF(0.78f, 0.82f, 0.56f);
            } else if (strstr(name, "type")) {
                color = D2D1::ColorF(0.86f, 0.60f, 0.45f);
            } else if (strstr(name, "string")) {
                color = D2D1::ColorF(0.65f, 0.82f, 0.56f);
            } else if (strstr(name, "comment")) {
                color = D2D1::ColorF(0.45f, 0.52f, 0.40f);
            } else if (strstr(name, "number")) {
                color = D2D1::ColorF(0.82f, 0.68f, 0.90f);
            } else if (strstr(name, "operator")) {
                color = D2D1::ColorF(0.75f, 0.75f, 0.78f);
            } else if (strstr(name, "constant")) {
                color = D2D1::ColorF(0.86f, 0.72f, 0.50f);
            } else if (strstr(name, "property")) {
                color = D2D1::ColorF(0.70f, 0.80f, 0.88f);
            } else if (strstr(name, "variable")) {
                color = D2D1::ColorF(0.88f, 0.88f, 0.88f);
            } else {
                continue;
            }

            ComPtr<ID2D1SolidColorBrush> brush;
            ctx2->CreateSolidColorBrush(color, brush.GetAddressOf());
            DWRITE_TEXT_RANGE range = { startChar, endChar - startChar };
            g_textLayout->SetDrawingEffect(brush.Get(), range);
        }
    }

    if (g_textLayout) {
        D2D1_RECT_F editorClip = D2D1::RectF(0, g_statusBarH + g_menuBarH, (float)g_windowW, (float)g_windowH - hintBarH);
        ctx->PushAxisAlignedClip(editorClip, D2D1_ANTIALIAS_MODE_ALIASED);

        ComPtr<ID2D1SolidColorBrush> textBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.88f, 0.88f, 0.88f), textBrush.GetAddressOf());

        ComPtr<ID2D1SolidColorBrush> selBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.45f, 0.70f, 0.4f), selBrush.GetAddressOf());

        ComPtr<ID2D1SolidColorBrush> caretBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.70f, 1.0f), caretBrush.GetAddressOf());

        if (g_selStart != g_selEnd) {
            size_t a = (std::min)(g_selStart, g_selEnd);
            size_t b = (std::max)(g_selStart, g_selEnd);

            UINT32 rangeLen = (UINT32)(b - a);
            UINT32 hitCount = 0;
            g_textLayout->HitTestTextRange((UINT32)a, rangeLen, 0, 0, nullptr, 0, &hitCount);

            if (hitCount > 0) {
                std::vector<DWRITE_HIT_TEST_METRICS> hits(hitCount);
                g_textLayout->HitTestTextRange((UINT32)a, rangeLen, 0, 0, hits.data(), hitCount, &hitCount);

                for (UINT32 i = 0; i < hitCount; i++) {
                    float sl = g_marginLeft + hits[i].left;
                    float st = g_marginTop + hits[i].top - g_scrollY;
                    float sr = g_marginLeft + hits[i].left + hits[i].width;
                    float sb = g_marginTop + hits[i].top + hits[i].height - g_scrollY;
                    ctx->FillRectangle(D2D1::RectF(sl, st, sr, sb), selBrush.Get());
                }
            }
        }

        D2D1_POINT_2F origin = { g_marginLeft, g_marginTop - g_scrollY };
        ctx->DrawTextLayout(origin, g_textLayout.Get(), textBrush.Get());

        if (g_caretVisible) {
            FLOAT cpx, cpy;
            DWRITE_HIT_TEST_METRICS hit;
            if (SUCCEEDED(g_textLayout->HitTestTextPosition((UINT32)g_cursorPos, FALSE, &cpx, &cpy, &hit))) {
                float cx = g_marginLeft + cpx;
                float cy = g_marginTop + cpy - g_scrollY;
                float ch = hit.height > 0 ? hit.height : g_fontSize * 1.15f;
                ctx->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx, cy + ch), caretBrush.Get(), 2.0f);
            }
        }

        ComPtr<ID2D1SolidColorBrush> lineNoBrush;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.35f, 0.38f), lineNoBrush.GetAddressOf());

        UINT32 lineCount = 0;
        g_textLayout->GetLineMetrics(nullptr, 0, &lineCount);
        if (lineCount > 0) {
            std::vector<DWRITE_LINE_METRICS> metrics(lineCount);
            g_textLayout->GetLineMetrics(metrics.data(), lineCount, &lineCount);

            size_t pos = 0;
            float lineY = 0.0f;
            for (UINT32 i = 0; i < lineCount; i++) {
                float ly = g_marginTop + lineY - g_scrollY;
                float lh = metrics[i].height;

                if (ly + lh >= 0 && ly <= (float)g_windowH) {
                    wchar_t num[8];
                    _snwprintf_s(num, 8, L"%u", i + 1);
                    ctx->DrawText(num, (UINT32)wcslen(num), g_lineNoFmt.Get(),
                        D2D1::RectF(4, ly, g_marginLeft - 8, ly + lh), lineNoBrush.Get(),
                        D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                }

                lineY += lh;
                pos += metrics[i].length;
                if (pos > g_text.length()) break;
            }
        }

        ctx->PopAxisAlignedClip();
    }

    ComPtr<ID2D1SolidColorBrush> statusBgBrush;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.14f, 0.16f), statusBgBrush.GetAddressOf());
    ctx->FillRectangle(D2D1::RectF(0, 0, (float)g_windowW, g_statusBarH), statusBgBrush.Get());

    ComPtr<ID2D1SolidColorBrush> menuBarBgBrush;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.20f), menuBarBgBrush.GetAddressOf());

    ComPtr<ID2D1SolidColorBrush> statusBrush;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.70f, 0.70f), statusBrush.GetAddressOf());

    double periodMs = g_dcomp->GetStatistics().framePeriod / 10000.0;
    double fps = (periodMs > 0) ? 1000.0 / periodMs : 0;

    wchar_t statusBuf[256];
    _snwprintf_s(statusBuf, 256,
        L"DComp Editor  |  %zu chars  |  Ln %zu  Col %zu  |  %.0f fps",
        g_text.length(),
        CountNewlinesBefore(g_cursorPos) + 1,
        g_cursorPos - LastNewlineBefore(g_cursorPos),
        fps);

    ComPtr<IDWriteTextFormat> statusFmt;
    g_dcomp->GetDWriteFactory()->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us",
        statusFmt.GetAddressOf());
    if (statusFmt) {
        statusFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        statusFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ctx->DrawText(statusBuf, (UINT32)wcslen(statusBuf), statusFmt.Get(),
            D2D1::RectF(12, 0, (float)g_windowW, g_statusBarH), statusBrush.Get());
    }

    ComPtr<ID2D1SolidColorBrush> separatorBrush;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.25f, 0.28f), separatorBrush.GetAddressOf());
    ctx->DrawLine(D2D1::Point2F(0, g_statusBarH), D2D1::Point2F((float)g_windowW, g_statusBarH), separatorBrush.Get(), 1.0f);

    float menuY = g_statusBarH;
    ctx->FillRectangle(D2D1::RectF(0, menuY, (float)g_windowW, menuY + g_menuBarH), menuBarBgBrush.Get());

    ComPtr<ID2D1SolidColorBrush> menuText;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.80f, 0.80f, 0.82f), menuText.GetAddressOf());
    ComPtr<ID2D1SolidColorBrush> menuTextActive;
    ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), menuTextActive.GetAddressOf());
    ComPtr<ID2D1SolidColorBrush> menuHighlight;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.37f, 0.60f), menuHighlight.GetAddressOf());

    for (int i = 0; i < g_menuCount; i++) {
        float mx = g_menus[i].x;
        float mw = g_menus[i].w;
        bool active = (i == g_menuOpen) || (i == g_menuHover && g_menuOpen < 0);

        if (active) {
            ctx->FillRectangle(D2D1::RectF(mx, menuY, mx + mw, menuY + g_menuBarH), menuHighlight.Get());
        }

        g_menuFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        ctx->DrawText(g_menus[i].label, (UINT32)wcslen(g_menus[i].label), g_menuFmt.Get(),
            D2D1::RectF(mx, menuY, mx + mw, menuY + g_menuBarH),
            active ? menuTextActive.Get() : menuText.Get());
    }

    ctx->DrawLine(D2D1::Point2F(0, menuY + g_menuBarH), D2D1::Point2F((float)g_windowW, menuY + g_menuBarH),
        separatorBrush.Get(), 1.0f);

    ComPtr<ID2D1SolidColorBrush> hintBrush;
    ctx->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.40f, 0.42f), hintBrush.GetAddressOf());
    ctx->FillRectangle(D2D1::RectF(0, (float)g_windowH - hintBarH, (float)g_windowW, (float)g_windowH), menuBarBgBrush.Get());
    const wchar_t* hint = L"DirectComposition + D2D + DWrite  |  Ctrl+A/X/C/V  |  Esc=quit";
    if (statusFmt) {
        ctx->DrawText(hint, (UINT32)wcslen(hint), statusFmt.Get(),
            D2D1::RectF(12, (float)g_windowH - hintBarH, (float)g_windowW, (float)g_windowH), hintBrush.Get());
    }

    if (g_menuOpen >= 0) {
        ComPtr<ID2D1SolidColorBrush> dropBg;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.20f), dropBg.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> dropHighlight;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.37f, 0.60f), dropHighlight.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> dropText;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.82f, 0.82f, 0.84f), dropText.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> dropShortcut;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.54f), dropShortcut.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> dropSep;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.28f, 0.32f), dropSep.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> dropBorder;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.30f, 0.35f), dropBorder.GetAddressOf());

        MenuDef& m = g_menus[g_menuOpen];
        float dropW = 240.0f;
        float dropX = m.x;
        float dropY = menuY + g_menuBarH;

        float totalH = 0;
        for (int i = 0; i < m.count; i++)
            totalH += m.items[i].separator ? 6.0f : 24.0f;

        ctx->FillRectangle(D2D1::RectF(dropX, dropY, dropX + dropW, dropY + totalH), dropBg.Get());
        ctx->DrawRectangle(D2D1::RectF(dropX, dropY, dropX + dropW, dropY + totalH), dropBorder.Get(), 1.0f);

        float iy = dropY;
        for (int i = 0; i < m.count; i++) {
            MenuItem& item = m.items[i];
            if (item.separator) {
                ctx->DrawLine(D2D1::Point2F(dropX + 8, iy + 3), D2D1::Point2F(dropX + dropW - 8, iy + 3),
                    dropSep.Get(), 1.0f);
                iy += 6.0f;
            } else {
                float ih = 24.0f;
                bool hovered = (i == g_dropHover);

                if (hovered) {
                    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(dropX + 2, iy + 1, dropX + dropW - 2, iy + ih - 1), 2, 2);
                    ctx->FillRoundedRectangle(rr, dropHighlight.Get());
                }

                g_menuFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                ctx->DrawText(item.label, (UINT32)wcslen(item.label), g_menuFmt.Get(),
                    D2D1::RectF(dropX + 12, iy, dropX + dropW - 80, iy + ih),
                    hovered ? menuTextActive.Get() : dropText.Get());

                if (item.shortcut[0]) {
                    ComPtr<IDWriteTextFormat> scFmt;
                    g_dcomp->GetDWriteFactory()->CreateTextFormat(
                        L"Segoe UI", nullptr,
                        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                        DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us",
                        scFmt.GetAddressOf());
                    if (scFmt) {
                        scFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        scFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                        ctx->DrawText(item.shortcut, (UINT32)wcslen(item.shortcut), scFmt.Get(),
                            D2D1::RectF(dropX + dropW - 75, iy, dropX + dropW - 8, iy + ih),
                            dropShortcut.Get());
                    }
                }

                iy += ih;
            }
        }
    }

    if (g_ctxOpen) {
        ComPtr<ID2D1SolidColorBrush> ctxBg;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.20f), ctxBg.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> ctxHighlight;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.37f, 0.60f), ctxHighlight.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> ctxText;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.82f, 0.82f, 0.84f), ctxText.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> ctxTextDim;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.45f, 0.48f), ctxTextDim.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> ctxSep;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.28f, 0.32f), ctxSep.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> ctxBorder;
        ctx->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.30f, 0.35f), ctxBorder.GetAddressOf());

        float dw = 200.0f;
        float totalH = 0;
        for (int i = 0; i < g_ctxItemCount; i++)
            totalH += g_ctxItems[i].separator ? 6.0f : 24.0f;

        ctx->FillRectangle(D2D1::RectF(g_ctxX, g_ctxY, g_ctxX + dw, g_ctxY + totalH), ctxBg.Get());
        ctx->DrawRectangle(D2D1::RectF(g_ctxX, g_ctxY, g_ctxX + dw, g_ctxY + totalH), ctxBorder.Get(), 1.0f);

        bool hasSel = (g_selStart != g_selEnd);

        float iy = g_ctxY;
        for (int i = 0; i < g_ctxItemCount; i++) {
            CtxMenuItem& item = g_ctxItems[i];
            if (item.separator) {
                ctx->DrawLine(D2D1::Point2F(g_ctxX + 8, iy + 3), D2D1::Point2F(g_ctxX + dw - 8, iy + 3),
                    ctxSep.Get(), 1.0f);
                iy += 6.0f;
            } else {
                float ih = 24.0f;
                bool hovered = (i == g_ctxHover);
                bool enabled = true;
                if (item.action == MA_CUT || item.action == MA_COPY)
                    enabled = hasSel;

                if (hovered) {
                    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(g_ctxX + 2, iy + 1, g_ctxX + dw - 2, iy + ih - 1), 2, 2);
                    ctx->FillRoundedRectangle(rr, ctxHighlight.Get());
                }

                g_menuFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                ctx->DrawText(item.label, (UINT32)wcslen(item.label), g_menuFmt.Get(),
                    D2D1::RectF(g_ctxX + 12, iy, g_ctxX + dw - 8, iy + ih),
                    enabled ? (hovered ? menuTextActive.Get() : ctxText.Get()) : ctxTextDim.Get());
                iy += ih;
            }
        }
    }

    hr = ctx->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        OutputDebugStringA("[Editor] D2D device lost, recreating\n");
    }

    ctx->SetTarget(nullptr);

    g_dcomp->GetSwapChain()->Present(1, 0);
    if (!g_dcomp->IsFirstPresentDone()) {
        g_dcomp->MarkFirstPresent();
        SetTimer(g_hwnd, 3, 100, nullptr);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DCompTextEditor";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor = nullptr;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    if (!RegisterClass(&wc)) { CoUninitialize(); return 1; }

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName,
        L"DirectComposition Text Editor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) { CoUninitialize(); return 1; }

    g_hwnd = hwnd;
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
