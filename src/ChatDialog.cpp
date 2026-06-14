#include "ChatDialog.h"
#include "utils.h"
#include "LAppPal.h"
#include <Richedit.h>
#include <sstream>

HMODULE ChatDialog::s_hRichEdit = nullptr;
bool ChatDialog::s_richEditLoaded = false;

void ChatDialog::LoadRichEdit() {
    if (!s_richEditLoaded) {
        s_hRichEdit = LoadLibraryW(L"Msftedit.dll");
        s_richEditLoaded = true;
    }
}

ChatDialog::~ChatDialog() {
    Close();
    if (s_hRichEdit) {
        FreeLibrary(s_hRichEdit);
        s_hRichEdit = nullptr;
    }
}

bool ChatDialog::Create(HWND hParent, HINSTANCE hInstance) {
    LoadRichEdit();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"Live2DChatDialog";
    wc.cbWndExtra = sizeof(LONG_PTR);

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            LAppPal::PrintLog("[ChatDialog] RegisterClassExW failed: %lu", err);
            return false;
        }
    }

    m_hwnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        L"Live2DChatDialog",
        L"Chat with Furina",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        420, 520,
        hParent,
        nullptr,
        hInstance,
        this
    );

    if (!m_hwnd) {
        LAppPal::PrintLog("[ChatDialog] CreateWindowExW failed: %lu", GetLastError());
        return false;
    }

    LAppPal::PrintLog("[ChatDialog] Window created successfully");
    return true;
}

void ChatDialog::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        m_visible = true;
    }
}

void ChatDialog::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
        m_visible = false;
    }
}

void ChatDialog::Close() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_visible = false;
}

// ── Static subclass procedure for Edit control ──────────────────────
LRESULT CALLBACK ChatDialog::InputSubclassProc(HWND hWnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        ChatDialog* dlg = (ChatDialog*)dwRefData;
        if (dlg) dlg->OnSend();
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

void ChatDialog::OnCreate(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Chat history - RichEdit read-only
    m_hChatHistory = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RichEdit20W",
        L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN,
        0, 0, 100, 100,
        hwnd, (HMENU)101, hInst, nullptr
    );
    SendMessageW(m_hChatHistory, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_hChatHistory, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));

    // Input field
    m_hInput = CreateWindowExW(
        0,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 100, 100,
        hwnd, (HMENU)102, hInst, nullptr
    );
    SendMessageW(m_hInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_hInput, EM_SETCUEBANNER, (WPARAM)FALSE,
                  (LPARAM)L"Type a message... Enter to send");

    // Subclass Edit control to catch Enter key (WM_KEYDOWN goes to child, not parent)
    SetWindowSubclass(m_hInput, InputSubclassProc, 1, (DWORD_PTR)this);

    // Send button
    m_hSendBtn = CreateWindowExW(
        0,
        L"BUTTON",
        L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 100,
        hwnd, (HMENU)IDOK, hInst, nullptr
    );
    SendMessageW(m_hSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Thinking indicator
    m_hThinkingLabel = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD,
        0, 0, 200, 20,
        hwnd, (HMENU)103, hInst, nullptr
    );
    SendMessageW(m_hThinkingLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    AppendText(L"Furina: Hi master! I'm here for you. Chat with me anytime~\r\n");
    AppendText(L"------------------------------------------------\r\n\r\n");

    SetFocus(m_hInput);
}

void ChatDialog::OnSize(int width, int height) {
    if (width < m_minWidth) width = m_minWidth;
    if (height < m_minHeight) height = m_minHeight;

    int inputAreaHeight = m_inputHeight + m_padding * 2;
    int historyHeight = height - inputAreaHeight;

    if (m_hChatHistory) {
        SetWindowPos(m_hChatHistory, nullptr,
                      m_padding, m_padding,
                      width - m_padding * 2, historyHeight - m_padding * 2,
                      SWP_NOZORDER);
    }
    if (m_hThinkingLabel) {
        SetWindowPos(m_hThinkingLabel, nullptr,
                      m_padding, historyHeight - m_padding - 18,
                      width - m_padding * 2, 20,
                      SWP_NOZORDER);
    }
    if (m_hInput) {
        SetWindowPos(m_hInput, nullptr,
                      m_padding, historyHeight + m_padding,
                      width - m_padding * 2 - m_btnWidth - m_padding,
                      m_inputHeight,
                      SWP_NOZORDER);
    }
    if (m_hSendBtn) {
        SetWindowPos(m_hSendBtn, nullptr,
                      width - m_padding - m_btnWidth, historyHeight + m_padding,
                      m_btnWidth, m_inputHeight + 2,
                      SWP_NOZORDER);
    }
}

void ChatDialog::OnSend() {
    wchar_t buf[4096] = {0};
    GetWindowTextW(m_hInput, buf, 4095);

    std::wstring wtext(buf);
    size_t start = wtext.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return;

    wtext = wtext.substr(start);
    size_t end = wtext.find_last_not_of(L" \t\r\n");
    if (end != std::wstring::npos) wtext = wtext.substr(0, end + 1);

    if (wtext.empty()) return;

    std::string text = utils::WideToUtf8(wtext);

    SetWindowTextW(m_hInput, L"");

    std::wstring userLine = L"You: " + wtext + L"\r\n";
    AppendText(userLine);

    SetThinking(true);

    LAppPal::PrintLog("[ChatDialog] OnSend: '%s'", text.c_str());

    if (m_sendCallback) {
        m_sendCallback(text);
    } else {
        LAppPal::PrintLog("[ChatDialog] WARNING: sendCallback is null!");
    }
}

void ChatDialog::AddMessage(const std::string& sender, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SetThinking(false);

    std::wstring line = utils::Utf8ToWide(sender + ": " + text + "\r\n");
    wchar_t* copy = _wcsdup(line.c_str());
    if (copy && m_hwnd) {
        PostMessageW(m_hwnd, WM_USER + 1, (WPARAM)copy, 0);
    }
}

void ChatDialog::SetThinking(bool thinking) {
    m_thinking = thinking;
    if (m_hThinkingLabel) {
        SetWindowTextW(m_hThinkingLabel, thinking ? L"Furina is typing..." : L"");
    }
}

void ChatDialog::AppendText(const std::wstring& text) {
    if (!m_hChatHistory) return;

    int len = GetWindowTextLengthW(m_hChatHistory);
    SendMessageW(m_hChatHistory, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(m_hChatHistory, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageW(m_hChatHistory, EM_SCROLLCARET, 0, 0);
}

LRESULT CALLBACK ChatDialog::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ChatDialog* self = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        self = (ChatDialog*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
        self->OnCreate(hwnd);
        return 0;
    }

    self = (ChatDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            self->OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK) {
                LAppPal::PrintLog("[ChatDialog] Send button clicked");
                self->OnSend();
                return 0;
            }
            break;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = self->m_minWidth;
            mmi->ptMinTrackSize.y = self->m_minHeight;
            return 0;
        }

        case WM_USER + 1: {
            wchar_t* pending = (wchar_t*)wParam;
            if (pending) {
                self->AppendText(pending);
                free(pending);
            }
            return 0;
        }

        case WM_CLOSE:
            self->Hide();
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
