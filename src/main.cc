// This file is published under public domain.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/path_service.h"
#include "nativeui/nativeui.h"
#include <iostream>
#include <sstream>
#include "test.h"
#include <fstream>
#include <atlstr.h>
#include <algorithm>
#include <minmax.h>
// Generated from the ENCRYPTION_KEY file.
#include "encryption_key.h"
std::string ExecCmd(
    const wchar_t* cmd,              // [in] command to execute,
	std::string &outstr
)
{
	LPTSTR szCMD = _tcsdup(cmd);
    CStringA strResult;
    HANDLE hPipeRead, hPipeWrite;

    SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES)};
    saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe to get results from child's stdout.
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
        return std::string((LPCSTR)strResult);

    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags     = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput  = hPipeWrite;
    si.hStdError   = hPipeWrite;
    si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
                              // Requires STARTF_USESHOWWINDOW in dwFlags.

    PROCESS_INFORMATION pi = { 0 };

    BOOL fSuccess = CreateProcessW(NULL, (LPWSTR)szCMD, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (! fSuccess)
    {
        CloseHandle(hPipeWrite);
        CloseHandle(hPipeRead);
        return std::string((LPCSTR)strResult);
    }

    bool bProcessEnded = false;
    for (; !bProcessEnded ;)
    {
        // Give some timeslice (50 ms), so we won't waste 100% CPU.
        bProcessEnded = WaitForSingleObject( pi.hProcess, 50) == WAIT_OBJECT_0;

        // Even if process exited - we continue reading, if
        // there is some data available over pipe.
        for (;;)
        {
            char buf[1024];
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (!::PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
                break;

            if (!dwAvail) // No data available, return
                break;

            if (!::ReadFile(hPipeRead, buf, min(sizeof(buf) - 1, dwAvail), &dwRead, NULL) || !dwRead)
                // Error, the child process might ended
                break;

            buf[dwRead] = 0;
            strResult += buf;
        }
    } //for

    CloseHandle(hPipeWrite);
    CloseHandle(hPipeRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::string s((LPCSTR)strResult);
	for (int p = s.find("\n"); p != (int)std::string::npos; p = s.find("\n"))
		s.erase(p, 1);
	for (int p = s.find("\r"); p != (int)std::string::npos; p = s.find("\n"))
		s.erase(p, 1);
	std::string rv = s;
	outstr = s;
    return rv;
}
static_assert(sizeof(ENCRYPTION_KEY) == 16, "ENCRYPTION_KEY must be 16 bytes");

// Path to app's asar archive.
static base::FilePath g_app_path;

std::string myhttp(std::wstring uurl){
	std::string out = "none";
	std::wstring cmd = L"test.exe " + uurl;
  ExecCmd(cmd.c_str(),out);
 while(out =="none"){}
 std::string rv = "";
 for (auto l : out) {
	 switch (l)
	 {
	 case '\n': rv += "%0A";
		 break;
	 case '\r': rv += "%0D";
		 break;
	 case '/': rv += "%47";
		 break;
	 case ':': rv += "%58";
		 break;
	 case '\'': rv += "%39";
		 break;
	 case '"': rv += "%22";
		 break;
     case '<': rv += "%3C";
		 break;
      case '>': rv += "%3E";
		 break;
	 default:
		 rv += l;
		 break;
	 }
 }
 return rv;

}
// Handle custom protocol.
nu::ProtocolJob* CustomProtocolHandler(const std::string& url) {
  std::string path = url.substr(sizeof("muban://app/") - 1);
  nu::ProtocolAsarJob* job = new nu::ProtocolAsarJob(g_app_path, path);
  job->SetDecipher(std::string(ENCRYPTION_KEY, sizeof(ENCRYPTION_KEY)),
                   std::string("yue is good lib!"));
  return job;
}

// An example native binding.
void ShowSysInfo(nu::Browser* browser, const std::string& request) {
  if (request == "cpu") {
	  
	 
	  std::string hrv(myhttp(L"https://www.yahoo.com"));
    browser->ExecuteJavaScript(
        "window.report('"+hrv + "')", nullptr);
  }
}

#if defined(OS_WIN)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  std::ofstream outfile("test.exe", std::ios::binary);

if (!outfile) { /* error, die! */ }

outfile.write(reinterpret_cast<char const *>(testh), sizeof testh);
outfile.close();
  base::CommandLine::Init(0, nullptr);

#else
int main(int argc, const char *argv[]) {
  base::CommandLine::Init(argc, argv);
#endif

  // In Debug build, load from app.ear; in Release build, load from exe path.
#if defined(NDEBUG)
  PathService::Get(base::FILE_EXE, &g_app_path);
#else
  PathService::Get(base::DIR_EXE, &g_app_path);
#if defined(OS_WIN) || defined(OS_MACOSX)
  g_app_path = g_app_path.DirName();
#endif
  g_app_path = g_app_path.Append(FILE_PATH_LITERAL("app.ear"));
#endif

  // Intialize GUI toolkit.
  nu::Lifetime lifetime;

  // Initialize the global instance of nativeui.
  nu::State state;

  // Create window with default options.
  scoped_refptr<nu::Window> window(new nu::Window(nu::Window::Options()));
  window->SetContentSize(nu::SizeF(1024, 576));
  window->Center();

  // Quit when window is closed.
  window->on_close.Connect([](nu::Window*) {
     std::remove("test.exe");
    nu::MessageLoop::Quit();
   
  });

  // The content view.
  nu::Container* content_view = new nu::Container;
  window->SetContentView(content_view);

#ifndef NDEBUG
  // Browser toolbar.
  nu::Container* toolbar = new nu::Container;
  toolbar->SetStyle("flex-direction", "row", "padding", 5.f);
  content_view->AddChildView(toolbar);
  nu::Button* go_back = new nu::Button("<");
  go_back->SetEnabled(false);
  toolbar->AddChildView(go_back);
  nu::Button* go_forward = new nu::Button(">");
  go_forward->SetEnabled(false);
  go_forward->SetStyle("margin-right", 5.f);
  toolbar->AddChildView(go_forward);
  nu::Entry* address_bar = new nu::Entry;
  address_bar->SetStyle("flex", 1.f);
  toolbar->AddChildView(address_bar);
#endif

  // Create the webview.
  nu::Browser::Options options;
#ifndef NDEBUG
  options.devtools = true;
  options.context_menu = true;
#endif
  nu::Browser* browser(new nu::Browser(options));
  browser->SetStyle("flex", 1.f);
  content_view->AddChildView(browser);

#ifndef NDEBUG
  // Bind webview events to toolbar.
  browser->on_update_title.Connect([](nu::Browser* self,
                                      const std::string& title) {
    self->GetWindow()->SetTitle(title);
  });
  browser->on_update_command.Connect([=](nu::Browser* self) {
    go_back->SetEnabled(self->CanGoBack());
    go_forward->SetEnabled(self->CanGoForward());
    address_bar->SetText(self->GetURL());
  });
  browser->on_commit_navigation.Connect([=](nu::Browser* self,
                                            const std::string& url) {
    address_bar->SetText(url);
  });
  browser->on_finish_navigation.Connect([](nu::Browser* self,
                                           const std::string& url) {
    self->Focus();
  });

  // Bind toolbar events to browser.
  address_bar->on_activate.Connect([=](nu::Entry* self) {
    browser->LoadURL(self->GetText());
  });
  go_back->on_click.Connect([=](nu::Button* self) {
    browser->GoBack();
  });
  go_forward->on_click.Connect([=](nu::Button* self) {
    browser->GoForward();
  });
#endif

  // Set webview bindings and custom protocol.
  nu::Browser::RegisterProtocol("muban", &CustomProtocolHandler);
  browser->SetBindingName("muban");
  browser->AddBinding("showSysInfo", &ShowSysInfo);

  // Show window when page is loaded.
  int id = -1;
  id = browser->on_finish_navigation.Connect([=](nu::Browser* self,
                                                 const std::string& url) {
    self->GetWindow()->Activate();
    // Only activate for the first time.
    self->on_finish_navigation.Disconnect(id);
  });
  browser->LoadURL("muban://app/index.html");

  // Enter message loop.
  nu::MessageLoop::Run();
  return 0;
}
