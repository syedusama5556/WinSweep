#define UNICODE
#define _UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")

// Universal Cache & Temp Cleaner (Windows 10/11)
// Build (MSVC): cl /std:c++17 /EHsc /DUNICODE /D_UNICODE UniversalCacheTempCleaner.cpp
// The program never preselects results. Folder deletions go to the Recycle Bin.
// Temp roots are preserved: only their children are recycled.

constexpr wchar_t APP_TITLE[] = L"Universal Cache & Temp Cleaner";
constexpr COLORREF BG=0x001E1E1E, SURFACE=0x002D2D30, TEXT=0x00F1F1F1, ACCENT=0x00007ACC;
enum { ID_SCAN=1001,ID_STOP,ID_DELETE,ID_CHECKALL,ID_LIST,ID_PROGRESS,ID_STATUS,
       ID_MIN,ID_DEEP,ID_PROJECTS,ID_SYSTEM,ID_DEV,ID_DOCKER,ID_SELECTED,ID_OPEN,ID_COPY };
constexpr UINT M_ADD=WM_APP+1,M_PROGRESS=WM_APP+2,M_DONE=WM_APP+3;

enum class Action { RecycleFolder, RecycleContents, Command };
struct Result { std::wstring path, category, risk, note, command; unsigned long long bytes=0; Action action=Action::RecycleFolder; };
struct Progress { unsigned long long dirs=0, found=0; std::wstring path; };
struct Done { unsigned long long dirs=0, found=0; double seconds=0; bool cancelled=false; };

static HWND win,listv,progress,status,scanBtn,stopBtn,deleteBtn,minEdit,deepCheck,projectCheck,systemCheck,devCheck,dockerCheck,selectedLabel;
static HBRUSH bgBrush,surfaceBrush; static HFONT font,boldFont;
static std::atomic_bool cancelScan=false, scanning=false;
static bool closeAfterScan=false;
static std::atomic_ullong dirsScanned=0, foundCount=0;
static std::mutex seenMutex; static std::unordered_set<std::wstring> seen;
static std::vector<std::unique_ptr<Result>> rows;

static std::wstring lower(std::wstring s){ for(auto& c:s)c=(wchar_t)towlower(c); return s; }
static std::wstring join(const std::wstring&a,const std::wstring&b){return a.empty()?b:(a.back()==L'\\'?a+b:a+L"\\"+b);}
static std::wstring parent(const std::wstring&p){auto n=p.find_last_of(L"\\/");return n==std::wstring::npos?L"":p.substr(0,n);}
static bool dirExists(const std::wstring&p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
static bool fileExists(const std::wstring&p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
static std::wstring env(const wchar_t*n){DWORD z=GetEnvironmentVariableW(n,nullptr,0);if(!z)return L"";std::vector<wchar_t>b(z);GetEnvironmentVariableW(n,b.data(),z);return b.data();}
static std::wstring fmt(unsigned long long n){const wchar_t*u[]={L"B",L"KB",L"MB",L"GB",L"TB"};double v=(double)n;int i=0;while(v>=1024&&i<4){v/=1024;i++;}std::wostringstream s;s<<std::fixed<<std::setprecision(i&&v<10?2:i?1:0)<<v<<L" "<<u[i];return s.str();}
static unsigned long long minBytes(){wchar_t b[32]{};GetWindowTextW(minEdit,b,32);double n=_wtof(b);return (unsigned long long)(std::max(0.0,n)*1048576.0);}
static bool marker(const std::wstring&d,std::initializer_list<const wchar_t*> names){for(auto n:names)if(fileExists(join(d,n)))return true;return false;}
static bool hasExtension(const std::wstring&d,const wchar_t*ext){WIN32_FIND_DATAW f{};HANDLE h=FindFirstFileW(join(d,std::wstring(L"*")+ext).c_str(),&f);if(h==INVALID_HANDLE_VALUE)return false;bool ok=false;do{if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){ok=true;break;}}while(FindNextFileW(h,&f));FindClose(h);return ok;}

static unsigned long long folderSize(const std::wstring&root){
 if(cancelScan)return 0; unsigned long long total=0; WIN32_FIND_DATAW f{};HANDLE h=FindFirstFileW(join(root,L"*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return 0;
 do{if(cancelScan)break;if(!wcscmp(f.cFileName,L".")||!wcscmp(f.cFileName,L".."))continue;if(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)continue;
  if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)total+=folderSize(join(root,f.cFileName));else{ULARGE_INTEGER n{};n.HighPart=f.nFileSizeHigh;n.LowPart=f.nFileSizeLow;total+=n.QuadPart;}
 }while(FindNextFileW(h,&f));FindClose(h);return total;
}

static void post(Result r,unsigned long long threshold,bool allowEmpty=false){
 if(r.action!=Action::Command){if(!dirExists(r.path))return;r.bytes=folderSize(r.path);if(r.bytes<threshold&&!allowEmpty)return;}
 std::wstring key=lower(r.action==Action::Command?r.command:r.path);{std::lock_guard<std::mutex>g(seenMutex);if(!seen.insert(key).second)return;}
 auto*p=new Result(std::move(r));foundCount++;if(!PostMessageW(win,M_ADD,0,(LPARAM)p))delete p;
}

static void knownTargets(unsigned long long m,bool sys,bool dev,bool docker){
 const auto local=env(L"LOCALAPPDATA"), roaming=env(L"APPDATA"), profile=env(L"USERPROFILE"), windir=env(L"WINDIR"), programData=env(L"PROGRAMDATA");
 auto add=[&](std::wstring p,const wchar_t*c,const wchar_t*r,const wchar_t*n,Action a=Action::RecycleContents){if(!p.empty())post({p,c,r,n,L"",0,a},m);};
 add(env(L"TEMP"),L"Windows / app temp",L"Low",L"Contents only; in-use files will remain");
 add(join(local,L"Temp"),L"Windows / app temp",L"Low",L"Contents only; in-use files will remain");
 add(join(local,L"CrashDumps"),L"Crash dumps",L"Low",L"Diagnostic crash dumps");
 add(join(local,L"D3DSCache"),L"Graphics shader cache",L"Low",L"Recreated by applications");
 add(join(local,L"Microsoft\\Windows\\INetCache"),L"Windows internet cache",L"Low",L"May require applications to be closed");
 add(join(local,L"Microsoft\\Windows\\Explorer"),L"Explorer thumbnail cache",L"Medium",L"Explorer may keep some files locked");
 add(join(local,L"Microsoft\\Windows\\WER"),L"Windows error reports",L"Low",L"Diagnostic reports");
 add(join(local,L"Google\\Chrome\\User Data\\Default\\Cache"),L"Chrome cache",L"Low",L"Close Chrome first");
 add(join(local,L"Google\\Chrome\\User Data\\Default\\Code Cache"),L"Chrome code cache",L"Low",L"Close Chrome first");
 add(join(local,L"Microsoft\\Edge\\User Data\\Default\\Cache"),L"Edge cache",L"Low",L"Close Edge first");
 add(join(local,L"Microsoft\\Edge\\User Data\\Default\\Code Cache"),L"Edge code cache",L"Low",L"Close Edge first");
 add(join(roaming,L"Code\\Cache"),L"VS Code cache",L"Low",L"Close VS Code first");
 add(join(roaming,L"Code\\CachedData"),L"VS Code cached data",L"Low",L"Extensions may recompile");
 if(dev){
  add(join(local,L"pip\\Cache"),L"Python pip cache",L"Low",L"Equivalent purpose to pip cache purge");
  add(join(roaming,L"npm-cache"),L"npm cache",L"Low",L"npm normally manages this cache itself");
  add(join(local,L"npm-cache"),L"npm cache",L"Low",L"Location used by newer/configured npm installs");
  add(join(local,L"Yarn\\Cache"),L"Yarn cache",L"Low",L"Packages will be downloaded again");
  add(join(local,L"pnpm-cache"),L"pnpm cache",L"Low",L"Packages may be downloaded again");
  add(join(local,L"NuGet\\v3-cache"),L"NuGet HTTP cache",L"Low",L"Packages will be downloaded again");
  add(join(profile,L".nuget\\packages"),L"NuGet global packages",L"Medium",L"Large; projects must restore packages again");
  add(join(profile,L".gradle\\caches"),L"Gradle user cache",L"Medium",L"Dependencies and transforms will be rebuilt");
  add(join(profile,L".gradle\\daemon"),L"Gradle daemon data",L"Low",L"Stop Gradle builds first");
  add(join(profile,L".m2\\repository"),L"Maven local repository",L"Medium",L"Dependencies will be downloaded again");
  add(join(profile,L".cargo\\registry\\cache"),L"Cargo registry cache",L"Low",L"Crates will be downloaded again");
  add(join(profile,L".cargo\\git\\checkouts"),L"Cargo git checkouts",L"Medium",L"Git dependencies will be cloned again");
  add(join(local,L"go-build"),L"Go build cache",L"Low",L"Equivalent purpose to go clean -cache");
  add(join(local,L"Pub\\Cache"),L"Dart / Flutter pub cache",L"Medium",L"Packages will be downloaded again");
  add(join(profile,L".android\\cache"),L"Android SDK cache",L"Low",L"Downloads may be repeated");
  add(join(local,L"JetBrains"),L"JetBrains local caches",L"Medium",L"Review product/version subfolders before selecting");
  add(join(local,L"Microsoft\\VisualStudio"),L"Visual Studio local cache",L"Medium",L"Review version subfolders before selecting");
  add(join(local,L"UnrealEngine\\Common\\DerivedDataCache"),L"Unreal derived data",L"Low",L"Assets/shaders will be regenerated");
  add(join(local,L"Unity\\cache"),L"Unity cache",L"Low",L"Assets may be reimported");
 }
 if(sys){
  add(join(windir,L"Temp"),L"Windows system temp",L"Medium",L"Administrator rights may be required");
  add(join(programData,L"Microsoft\\Windows\\WER"),L"System error reports",L"Low",L"Administrator rights may be required");
  add(join(programData,L"Microsoft\\Windows\\DeliveryOptimization\\Cache"),L"Delivery Optimization cache",L"Medium",L"Prefer Windows Storage settings if deletion fails");
 }
 if(docker){
  if(GetFileAttributesW(L"C:\\Program Files\\Docker\\Docker\\resources\\bin\\docker.exe")!=INVALID_FILE_ATTRIBUTES||SearchPathW(nullptr,L"docker.exe",nullptr,0,nullptr,nullptr)>0){
   post({L"Docker Desktop / Engine",L"Docker build cache",L"Low",L"Removes unused build cache through Docker",L"docker builder prune -f",0,Action::Command},0,true);
   post({L"Docker Desktop / Engine",L"Docker unused objects",L"Medium",L"Stopped containers, unused networks, dangling images and build cache; volumes excluded",L"docker system prune -f",0,Action::Command},0,true);
  }
 }
}

static bool skipTreeName(const std::wstring&n){static const wchar_t*x[]={L"Windows",L"WinSxS",L"Program Files",L"Program Files (x86)",L"ProgramData",L"AppData",L"$Recycle.Bin",L"System Volume Information",L"Recovery",L".git",L".svn"};for(auto s:x)if(!_wcsicmp(n.c_str(),s))return true;return false;}
static void scanTree(const std::wstring&root,unsigned long long m){
 if(cancelScan)return;WIN32_FIND_DATAW f{};HANDLE h=FindFirstFileW(join(root,L"*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return;
 do{if(cancelScan)break;if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT||!wcscmp(f.cFileName,L".")||!wcscmp(f.cFileName,L".."))continue;
  std::wstring n=f.cFileName,p=join(root,n),q=lower(n);auto d=++dirsScanned;if(d%200==0){auto*x=new Progress{d,foundCount.load(),p};if(!PostMessageW(win,M_PROGRESS,0,(LPARAM)x))delete x;}
  bool hit=false;
  if(q==L"node_modules"&&marker(root,{L"package.json"})){post({p,L"JavaScript dependencies",L"Medium",L"Recreate with npm/yarn/pnpm install",L"",0,Action::RecycleFolder},m);hit=true;}
  else if(q==L"build"&&marker(root,{L"pubspec.yaml",L"build.gradle",L"build.gradle.kts",L"CMakeLists.txt"})){post({p,L"Generated build output",L"Low",L"Recreated by the build system",L"",0,Action::RecycleFolder},m);hit=true;}
  else if(q==L".dart_tool"&&marker(root,{L"pubspec.yaml"})){post({p,L"Dart project cache",L"Low",L"Recreated by Flutter/Dart",L"",0,Action::RecycleFolder},m);hit=true;}
  else if(q==L".gradle"&&marker(root,{L"settings.gradle",L"settings.gradle.kts",L"build.gradle",L"build.gradle.kts"})){post({p,L"Gradle project cache",L"Low",L"Recreated by Gradle",L"",0,Action::RecycleFolder},m);hit=true;}
  else if(q==L"target"&&marker(root,{L"Cargo.toml"})){post({p,L"Rust build output",L"Low",L"Recreated by Cargo",L"",0,Action::RecycleFolder},m);hit=true;}
  else if((q==L"bin"||q==L"obj")&&(hasExtension(root,L".csproj")||hasExtension(root,L".fsproj")||hasExtension(root,L".vbproj"))){post({p,L".NET build output",L"Low",L"Recreated by dotnet/MSBuild",L"",0,Action::RecycleFolder},m);hit=true;}
  else if(q==L"__pycache__"||q==L".pytest_cache"||q==L".mypy_cache"||q==L".ruff_cache"){post({p,L"Python project cache",L"Low",L"Recreated by Python tooling",L"",0,Action::RecycleFolder},m);hit=true;}
  if(!hit&&!skipTreeName(n))scanTree(p,m);
 }while(FindNextFileW(h,&f));FindClose(h);
}

static std::vector<std::wstring> drives(){std::vector<std::wstring>r;DWORD z=GetLogicalDriveStringsW(0,nullptr);std::vector<wchar_t>b(z+2);if(!GetLogicalDriveStringsW((DWORD)b.size(),b.data()))return r;for(auto p=b.data();*p;p+=wcslen(p)+1)if(GetDriveTypeW(p)==DRIVE_FIXED)r.emplace_back(p);return r;}
static void start(bool deep,bool projects,bool sys,bool dev,bool docker,unsigned long long m){
 if(scanning.exchange(true))return;cancelScan=false;dirsScanned=foundCount=0;{std::lock_guard<std::mutex>g(seenMutex);seen.clear();}auto began=std::chrono::steady_clock::now();
 std::thread([=]{knownTargets(m,sys,dev,docker);if(projects&&!deep){auto p=env(L"USERPROFILE");if(!p.empty())scanTree(p,m);}if(deep){auto roots=drives();for(auto&d:roots){if(cancelScan)break;scanTree(d,m);}}auto end=std::chrono::steady_clock::now();auto*x=new Done{dirsScanned.load(),foundCount.load(),std::chrono::duration<double>(end-began).count(),cancelScan.load()};scanning=false;if(!PostMessageW(win,M_DONE,0,(LPARAM)x))delete x;}).detach();
}

static bool recycle(const std::wstring&p){std::vector<wchar_t>x(p.begin(),p.end());x.push_back(0);x.push_back(0);SHFILEOPSTRUCTW o{};o.hwnd=win;o.wFunc=FO_DELETE;o.pFrom=x.data();o.fFlags=FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;return SHFileOperationW(&o)==0&&!o.fAnyOperationsAborted;}
static bool recycleContents(const std::wstring&p){bool any=false,ok=true;WIN32_FIND_DATAW f{};HANDLE h=FindFirstFileW(join(p,L"*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return true;do{if(!wcscmp(f.cFileName,L".")||!wcscmp(f.cFileName,L".."))continue;any=true;if(!recycle(join(p,f.cFileName)))ok=false;}while(FindNextFileW(h,&f));FindClose(h);return !any||ok;}
static bool runCommand(const std::wstring&cmd){std::wstring args=L"/d /c "+cmd;SHELLEXECUTEINFOW s{sizeof(s)};s.fMask=SEE_MASK_NOCLOSEPROCESS;s.hwnd=win;s.lpVerb=L"open";s.lpFile=L"cmd.exe";s.lpParameters=args.c_str();s.nShow=SW_SHOW;if(!ShellExecuteExW(&s))return false;WaitForSingleObject(s.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(s.hProcess,&code);CloseHandle(s.hProcess);return code==0;}
static void selectedInfo(){int c=ListView_GetItemCount(listv),n=0;unsigned long long b=0;for(int i=0;i<c;i++)if(ListView_GetCheckState(listv,i)){n++;LVITEMW x{};x.mask=LVIF_PARAM;x.iItem=i;ListView_GetItem(listv,&x);if(x.lParam)b+=((Result*)x.lParam)->bytes;}std::wostringstream s;s<<L"Selected: "<<n<<L"  |  Measured space: "<<fmt(b);SetWindowTextW(selectedLabel,s.str().c_str());}
static void removeSelected(){
 std::vector<int>ids;for(int i=0;i<ListView_GetItemCount(listv);i++)if(ListView_GetCheckState(listv,i))ids.push_back(i);if(ids.empty()){MessageBoxW(win,L"Check one or more rows first.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;}
 bool hasMedium=false;for(int i:ids){LVITEMW x{};x.mask=LVIF_PARAM;x.iItem=i;ListView_GetItem(listv,&x);if(((Result*)x.lParam)->risk!=L"Low")hasMedium=true;}
 std::wostringstream q;q<<L"Clean "<<ids.size()<<L" selected item(s)?\n\nFolders go to the Recycle Bin. Managed Docker actions cannot be undone.";if(hasMedium)q<<L"\n\nThe selection includes Medium-risk items. Review their notes first.";if(MessageBoxW(win,q.str().c_str(),APP_TITLE,MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
 int ok=0,fail=0;for(auto it=ids.rbegin();it!=ids.rend();++it){LVITEMW x{};x.mask=LVIF_PARAM;x.iItem=*it;ListView_GetItem(listv,&x);auto*r=(Result*)x.lParam;bool done=r->action==Action::Command?runCommand(r->command):r->action==Action::RecycleContents?recycleContents(r->path):recycle(r->path);if(done){ListView_DeleteItem(listv,*it);ok++;}else fail++;}std::wostringstream s;s<<L"Cleaned: "<<ok<<L"  |  Failed or locked: "<<fail;SetWindowTextW(status,s.str().c_str());selectedInfo();
}

static void controls(bool active){EnableWindow(scanBtn,!active);EnableWindow(stopBtn,active);EnableWindow(deleteBtn,!active);EnableWindow(minEdit,!active);EnableWindow(deepCheck,!active);EnableWindow(projectCheck,!active);EnableWindow(systemCheck,!active);EnableWindow(devCheck,!active);EnableWindow(dockerCheck,!active);}
static void layout(HWND h){RECT r;GetClientRect(h,&r);int W=r.right,H=r.bottom,m=14,y=m,x=m;auto mv=[&](HWND w,int z){MoveWindow(w,x,y,z,30,TRUE);x+=z+7;};mv(scanBtn,90);mv(stopBtn,70);mv(deleteBtn,130);mv(GetDlgItem(h,ID_CHECKALL),95);x+=10;MoveWindow(GetDlgItem(h,2000),x,y+5,68,22,TRUE);x+=68;MoveWindow(minEdit,x,y+2,60,25,TRUE);x+=65;MoveWindow(GetDlgItem(h,2001),x,y+5,40,22,TRUE);y+=39;x=m;auto ck=[&](HWND w,int z){MoveWindow(w,x,y,z,24,TRUE);x+=z;};ck(devCheck,175);ck(systemCheck,160);ck(projectCheck,180);ck(deepCheck,190);ck(dockerCheck,150);y+=30;MoveWindow(selectedLabel,m,y,W-2*m,23,TRUE);y+=27;int bottom=48;MoveWindow(listv,m,y,W-2*m,std::max(100,H-y-bottom-m),TRUE);MoveWindow(progress,m,H-45,W-2*m,6,TRUE);MoveWindow(status,m,H-34,W-2*m,24,TRUE);int lw=W-2*m;ListView_SetColumnWidth(listv,0,std::max(250,lw-650));ListView_SetColumnWidth(listv,1,165);ListView_SetColumnWidth(listv,2,75);ListView_SetColumnWidth(listv,3,90);ListView_SetColumnWidth(listv,4,310);}
static HWND button(HWND h,const wchar_t*t,int id){HWND w=CreateWindowW(L"BUTTON",t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,h,(HMENU)id,nullptr,nullptr);SendMessageW(w,WM_SETFONT,(WPARAM)font,TRUE);return w;}
static HWND check(HWND h,const wchar_t*t,int id,bool on){HWND w=CreateWindowW(L"BUTTON",t,WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,0,0,0,h,(HMENU)id,nullptr,nullptr);SendMessageW(w,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);return w;}

static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
 switch(msg){case WM_CREATE:{win=h;font=CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");boldFont=CreateFontW(-13,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");bgBrush=CreateSolidBrush(BG);surfaceBrush=CreateSolidBrush(SURFACE);scanBtn=button(h,L"Scan",ID_SCAN);stopBtn=button(h,L"Stop",ID_STOP);deleteBtn=button(h,L"Clean selected",ID_DELETE);button(h,L"Check all",ID_CHECKALL);CreateWindowW(L"STATIC",L"Min size:",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)2000,nullptr,nullptr);minEdit=CreateWindowW(L"EDIT",L"10",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,0,0,0,0,h,(HMENU)ID_MIN,nullptr,nullptr);CreateWindowW(L"STATIC",L"MB",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)2001,nullptr,nullptr);
  devCheck=check(h,L"Developer caches",ID_DEV,true);systemCheck=check(h,L"System caches",ID_SYSTEM,true);projectCheck=check(h,L"Project folders",ID_PROJECTS,true);deepCheck=check(h,L"Deep scan all drives",ID_DEEP,false);dockerCheck=check(h,L"Docker actions",ID_DOCKER,true);
  selectedLabel=CreateWindowW(L"STATIC",L"Nothing selected",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)ID_SELECTED,nullptr,nullptr);SendMessageW(selectedLabel,WM_SETFONT,(WPARAM)boldFont,TRUE);
  listv=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,0,0,0,0,h,(HMENU)ID_LIST,nullptr,nullptr);ListView_SetExtendedListViewStyle(listv,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);const wchar_t*names[]={L"Path / managed target",L"Category",L"Risk",L"Size",L"What happens"};int widths[]={450,165,75,90,310};for(int i=0;i<5;i++){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(wchar_t*)names[i];c.cx=widths[i];ListView_InsertColumn(listv,i,&c);}
  progress=CreateWindowW(PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)ID_PROGRESS,nullptr,nullptr);SendMessageW(progress,PBM_SETBARCOLOR,0,ACCENT);status=CreateWindowW(L"STATIC",L"Ready. Nothing is selected automatically.",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)ID_STATUS,nullptr,nullptr);for(HWND c=GetWindow(h,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT))SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);controls(false);layout(h);return 0;}
 case WM_SIZE:layout(h);return 0;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{HDC d=(HDC)w;SetTextColor(d,TEXT);SetBkColor(d,BG);return (LRESULT)bgBrush;}case WM_CTLCOLOREDIT:{HDC d=(HDC)w;SetTextColor(d,TEXT);SetBkColor(d,SURFACE);return (LRESULT)surfaceBrush;}
 case WM_COMMAND:switch(LOWORD(w)){case ID_SCAN:ListView_DeleteAllItems(listv);rows.clear();controls(true);SetWindowTextW(status,L"Scanning known locations...");SendMessageW(progress,PBM_SETRANGE32,0,100);start(SendMessageW(deepCheck,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(projectCheck,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(systemCheck,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(devCheck,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(dockerCheck,BM_GETCHECK,0,0)==BST_CHECKED,minBytes());return 0;case ID_STOP:cancelScan=true;EnableWindow(stopBtn,false);SetWindowTextW(status,L"Stopping...");return 0;case ID_DELETE:removeSelected();return 0;case ID_CHECKALL:{bool all=true;for(int i=0;i<ListView_GetItemCount(listv);i++)if(!ListView_GetCheckState(listv,i)){all=false;break;}for(int i=0;i<ListView_GetItemCount(listv);i++)ListView_SetCheckState(listv,i,!all);selectedInfo();return 0;}}break;
 case M_ADD:{std::unique_ptr<Result>r((Result*)l);int i=ListView_GetItemCount(listv);LVITEMW x{};x.mask=LVIF_TEXT|LVIF_PARAM;x.iItem=i;x.pszText=(wchar_t*)r->path.c_str();x.lParam=(LPARAM)r.get();ListView_InsertItem(listv,&x);ListView_SetItemText(listv,i,1,(wchar_t*)r->category.c_str());ListView_SetItemText(listv,i,2,(wchar_t*)r->risk.c_str());std::wstring z=r->action==Action::Command?L"Managed":fmt(r->bytes);ListView_SetItemText(listv,i,3,z.data());ListView_SetItemText(listv,i,4,(wchar_t*)r->note.c_str());rows.push_back(std::move(r));std::wostringstream s;s<<L"Scanning... found "<<rows.size()<<L" items";SetWindowTextW(status,s.str().c_str());return 0;}
 case M_PROGRESS:{std::unique_ptr<Progress>p((Progress*)l);SendMessageW(progress,PBM_SETPOS,p->dirs%100,0);std::wostringstream s;s<<L"Scanning "<<p->dirs<<L" folders | found "<<p->found<<L" | "<<p->path;SetWindowTextW(status,s.str().c_str());return 0;}
 case M_DONE:{std::unique_ptr<Done>d((Done*)l);controls(false);SendMessageW(progress,PBM_SETPOS,100,0);if(closeAfterScan){DestroyWindow(h);return 0;}std::wostringstream s;s<<(d->cancelled?L"Scan stopped":L"Scan complete")<<L" | "<<d->dirs<<L" folders inspected | "<<rows.size()<<L" items | "<<std::fixed<<std::setprecision(1)<<d->seconds<<L" s";SetWindowTextW(status,s.str().c_str());return 0;}
 case WM_NOTIFY:{auto*n=(NMHDR*)l;if(n->hwndFrom==listv&&n->code==LVN_ITEMCHANGED){auto*v=(NMLISTVIEW*)l;if((v->uChanged&LVIF_STATE)&&((v->uOldState&LVIS_STATEIMAGEMASK)!=(v->uNewState&LVIS_STATEIMAGEMASK)))selectedInfo();}if(n->hwndFrom==listv&&n->code==NM_RCLICK){int i=ListView_GetNextItem(listv,-1,LVNI_SELECTED);if(i>=0){LVITEMW x{};x.mask=LVIF_PARAM;x.iItem=i;ListView_GetItem(listv,&x);auto*r=(Result*)x.lParam;POINT p;GetCursorPos(&p);HMENU m=CreatePopupMenu();if(r->action!=Action::Command)AppendMenuW(m,MF_STRING,ID_OPEN,L"Open containing folder");AppendMenuW(m,MF_STRING,ID_COPY,L"Copy path / command");int c=TrackPopupMenu(m,TPM_RETURNCMD,p.x,p.y,0,h,nullptr);DestroyMenu(m);if(c==ID_OPEN)ShellExecuteW(nullptr,L"open",L"explorer.exe",r->path.c_str(),nullptr,SW_SHOW);if(c==ID_COPY&&OpenClipboard(h)){EmptyClipboard();std::wstring t=r->action==Action::Command?r->command:r->path;HGLOBAL g=GlobalAlloc(GMEM_MOVEABLE,(t.size()+1)*sizeof(wchar_t));if(g){auto*q=(wchar_t*)GlobalLock(g);wcscpy_s(q,t.size()+1,t.c_str());GlobalUnlock(g);SetClipboardData(CF_UNICODETEXT,g);}CloseClipboard();}}return 0;}break;}
 case WM_CLOSE:if(scanning){closeAfterScan=true;cancelScan=true;ShowWindow(h,SW_HIDE);return 0;}DestroyWindow(h);return 0;case WM_DESTROY:cancelScan=true;DeleteObject(bgBrush);DeleteObject(surfaceBrush);DeleteObject(font);DeleteObject(boldFont);PostQuitMessage(0);return 0;}return DefWindowProcW(h,msg,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int show){INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&ic);WNDCLASSEXW c{sizeof(c)};c.lpfnWndProc=proc;c.hInstance=hi;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=LoadIconW(nullptr,IDI_APPLICATION);c.hbrBackground=CreateSolidBrush(BG);c.lpszClassName=L"UniversalCacheTempCleaner";if(!RegisterClassExW(&c))return 1;HWND h=CreateWindowExW(0,c.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1250,760,nullptr,nullptr,hi,nullptr);if(!h)return 1;ShowWindow(h,show);UpdateWindow(h);MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return(int)m.wParam;}
