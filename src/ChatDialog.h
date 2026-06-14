#pragma once

#include <string>
#include <vector>
#include <Windows.h>
#include <CommCtrl.h>
#include <functional>
#include <mutex>

class ChatDialog {
public:
    using SendCallback = std::function<void(const std::string& userMessage)>;

    ChatDialog() = default;
    ~ChatDialog();

    ChatDialog(const ChatDialog&) = delete;
    ChatDialog& operator=(const ChatDialog&) = delete;

    bool Create(HWND hParent, HINSTANCE hInstance);
    void Show();
    void Hide();
    void Close();

    HWND GetHwnd() const { return m_hwnd; }
    bool IsVisible() const { return m_visible; }

    // Called from any thread to add a message to the chat
    void AddMessage(const std::string& sender, const std::string& text);
    // Called when AI is thinking/processing
    void SetThinking(bool thinking);

    void SetSendCallback(SendCallback callback) { m_sendCallback = callback; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK InputSubclassProc(HWND hWnd, UINT msg, WPARAM wParam,
                                               LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    void OnCreate(HWND hwnd);
    void OnSize(int width, int height);
    void OnSend();
    void AppendText(const std::wstring& text);

    HWND m_hwnd = nullptr;
    HWND m_hChatHistory = nullptr;
    HWND m_hInput = nullptr;
    HWND m_hSendBtn = nullptr;
    HWND m_hThinkingLabel = nullptr;
    bool m_visible = false;
    bool m_thinking = false;

    SendCallback m_sendCallback;
    std::mutex m_mutex;

    int m_historyHeight = 400;
    int m_inputHeight = 25;
    int m_btnWidth = 60;
    int m_padding = 8;
    int m_minWidth = 380;
    int m_minHeight = 300;

    // Rich edit library handle
    static HMODULE s_hRichEdit;
    static bool s_richEditLoaded;
    static void LoadRichEdit();
};
