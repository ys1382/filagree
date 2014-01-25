
/* 
 * File:   hal_windows.c
 * Author: Sinbad Flyce
 *
 * Created on November 19, 2013, 11:23 AM
 */

#include <stdbool.h>
#include <inttypes.h>
#include "struct.h"
#include "file.h"
#include "vm.h"
#include "hal.h"
#include "compile.h"
#include <windows.h>
#include <unistd.h>
#include "sys.h"

#ifndef NO_UI
#include <w32api.h>
#include <commctrl.h>
#include <shellapi.h>
#endif

/* 
 * Constance
 */                         
#define ID_LISTVIEW      1000
#define MAX_ITEMLEN      64
#define MAX_ID           12
#define MAX_CMD          64
#define MARGIN_X 2
#define MARGIN_Y 2
#define BUTTON_ID       1000
#define CONTROL_HEIGH   30

static UINT WMU_RELOAD_LISTVIEW = WM_USER + 0x100;

/* 
 * Actionifier
 */
typedef struct tagActionifier {
    struct variable *logic;
    struct context *context;
    struct variable *uictx;
    struct variable *param;
    struct variable *data;
    DWORD timer;
} Actionifier;

/* 
 * file_thread
 */
struct file_thread {
    struct context *context;
    struct variable *listener;
    const char *watched;
};

#ifndef NO_UI
/* 
 * IMPLEMENTATION HAL
 * WIN32
 * We use hWnd for filagree's context
 */
static HWND hWndMain = NULL;
static HINSTANCE hInstance = NULL;

/* 
 * Local functions
 */
static HWND CreateListView (HWND hWndParent, Actionifier *act);
static void OnPress(HWND hWnd, Actionifier *act);
static void ActCallback(Actionifier *act);
static void ReloadListView(HWND hList,Actionifier *act);
static char *ExecuteAppendPath(const char *stringAppend);
static void ShowDetailWindow(const char *file);
static void OpenWithFile(const char *file);

#endif

/* 
 * actionifier_new
 */
Actionifier * actionifier_new(struct context *f, struct variable *u, struct variable *c, struct variable *d)
{
    Actionifier *bp = (Actionifier *)malloc(sizeof(Actionifier));
    bp->logic = c;
    bp->context = f;
    bp->uictx = u;
    bp->data = d;
    bp->param = NULL;
    bp->timer = 0;
    return bp;
}

/* 
 * whereAmI
 */
RECT whereAmI(int x, int y, int w, int h)
{
    RECT rcResult = {x + MARGIN_X, y + MARGIN_Y, x + w + MARGIN_X, y + h + MARGIN_Y};
    return rcResult;
}

/* 
 * resize
 */
void resize(HWND control, int32_t *w, int32_t *h)
{
    if (*w && *h)
        return;
    
    RECT rcClient = {0};
    GetClientRect(control, &rcClient);
    
    *w = (rcClient.right - rcClient.left);
    *h = (rcClient.bottom - rcClient.top);
    RECT rect = whereAmI(0,0, *w,*h);
    MoveWindow(control,0, 0,rect.right - rect.left, rect.bottom - rect.top, TRUE);
}

/* 
 * hal_save
 */
void hal_save(struct context *context, const struct byte_array *key, const struct variable *value)
{    
}

/* 
 * hal_load
 */
struct variable *hal_load(struct context *context, const struct byte_array *key)
{    
}

/* 
 * hal_sleep
 */
void hal_sleep(int32_t miliseconds)
{
    struct timespec req={0};
    time_t sec = (int)(miliseconds/1000);
    miliseconds = (int32_t)(miliseconds - (sec * 1000));
    req.tv_sec = sec;
    req.tv_nsec = miliseconds * 1000000L;
    while (nanosleep(&req,&req) == -1)
        continue;    
}

/* 
 * hal_timer
 */
void hal_timer(struct context *context,
               int32_t milliseconds,
               struct variable *logic,
               bool repeats)
{   
}

/* 
 * trim_last_commnad_path
 */
char* trim_last_commnad_path(char *path)
{
    char *pch = strrchr(path,'/');
    if (pch != NULL) {
        int ls = pch - path;
        path[ls] = 0;
        path[ls + 1] = 0;
    }
    return path;
}

/* 
 * path_var
 */
struct variable *path_var(struct file_thread *thread, const char *path)
{
    int lenp = strlen(path);
    int lenw = strlen(thread->watched);
    char* path2 = (char*)thread->watched;
    
    if (lenp > lenw) {
        path2 = (char*)path + lenw + 1;
    }
    char *triml = trim_last_commnad_path(path2);
    return NULL;
}

/* 
 * 
 */
void file_listener_callback(char *path, void *clientCallBackInfo)
{ 
    DEBUGPRINT("file_listener_callback:\n");
    
    struct file_thread *thread = (struct file_thread*)clientCallBackInfo;
    
    gil_lock(thread->context, "file_listener_callback");
    
    struct variable *path3 = path_var(thread, path);
    struct variable *method2 = event_string(thread->context, FILED);
    struct variable *method3 = variable_map_get(thread->context, thread->listener, method2);
    
    if ((method3 != NULL) && (method3->type != VAR_NIL))
        vm_call(thread->context, method3, thread->listener, path3);
    
    gil_unlock(thread->context, "file_listener_callback");
}

/* 
 * hal_file_listen
 */
void *thread_file_listen(void *param)
{   
    //return ;
    struct file_thread *thread = (struct file_thread*)param;
    const char *pathToWatch = thread->watched;

    HANDLE hDir = CreateFile( 
            pathToWatch, /* pointer to the file name */
            FILE_LIST_DIRECTORY,                /* access (read-write) mode */
            FILE_SHARE_READ|FILE_SHARE_DELETE | FILE_SHARE_WRITE,  /* share mode */
            NULL, /* security descriptor */
            OPEN_EXISTING, /* how to create */
            FILE_FLAG_BACKUP_SEMANTICS, /* file attributes */
            NULL /* file with attributes to copy */
            );
        
    if (hDir == INVALID_HANDLE_VALUE) 
    {
        DEBUGPRINT("thread_file_listen : %s","error");
        return NULL;
    }
    
    FILE_NOTIFY_INFORMATION Buffer[1024];
    DWORD BytesReturned;
    
    while( ReadDirectoryChangesW(
            hDir, /* handle to directory */
            &Buffer, /* read results buffer */
            sizeof(Buffer), /* length of buffer */
            TRUE, /* monitoring option */
            FILE_NOTIFY_CHANGE_SECURITY|
            FILE_NOTIFY_CHANGE_CREATION|
            FILE_NOTIFY_CHANGE_LAST_ACCESS|
            FILE_NOTIFY_CHANGE_LAST_WRITE|
            FILE_NOTIFY_CHANGE_SIZE|
            FILE_NOTIFY_CHANGE_ATTRIBUTES|
            FILE_NOTIFY_CHANGE_DIR_NAME|
            FILE_NOTIFY_CHANGE_FILE_NAME, /* filter conditions */
            &BytesReturned, /* bytes returned */
            NULL, /* overlapped buffer */
            NULL))
    {
        int i = 0;
        do
        {
            char cfilename[256] = {0};
            char fileEvent[256] = {0};
            wcstombs(cfilename,Buffer[i].FileName,Buffer[i].FileNameLength/2);
            
            sprintf(fileEvent,"%s/%s",pathToWatch,cfilename);
            file_listener_callback(fileEvent,param);
            i++;
            
            DEBUGPRINT("FileEvent : %s",fileEvent);
        }
        while (!Buffer[i].NextEntryOffset);
    }
    
    CloseHandle(hDir);
    
	return NULL;    
}

/* 
 * hal_file_listen
 */
void hal_file_listen(struct context *context, const char *path, struct variable *listener)
{
    //return;
#ifdef NO_UI    
    pthread_t file_listen_thread;
    struct file_thread *ft = (struct file_thread*)malloc((sizeof(struct file_thread)));
    ft->context = context;
    ft->listener = listener;
    ft->watched = path;
    
    pthread_create(&file_listen_thread,NULL,&thread_file_listen,ft);
    //pthread_join(file_listen_thread,NULL); 
#endif    
}
/* 
 * hal_loop
 */
void hal_loop(struct context *context)
{   
 #ifdef NO_UI
    gil_unlock(context,"loop");
        MSG msg = {0};
    HACCEL hAccelTable = NULL;
    // Main message loop.
    while(GetMessage(&msg, NULL, 0, 0) > 0){
        if (! TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
    }
#endif    
}

const char *hal_doc_path(const struct byte_array *path)
{
    return path ? byte_array_to_string(path) : NULL;
}

bool hal_open(const char *path)
{
#ifndef NO_UI
    OpenWithFile(path);
#endif    
    return true;
}

#ifndef NO_UI

/* 
 * hal_image
 */
void hal_image()
{   
}

/* 
 * hal_sound
 */
void hal_sound()
{    
}

/* 
 * hal_audioloop
 */
void hal_audioloop()
{
}

/* 
 * hal_graphics
 */
void hal_graphics(const struct variable *shape)
{
}

/* 
 * hal_synth
 */
void hal_synth(const uint8_t *bytes, uint32_t length)
{    
}

/* 
 * hal_window
 */
void *hal_window(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic)
{
    // Remove all sub-windows
    HWND child = FindWindowEx(hWndMain,NULL,NULL,NULL);
    while (child != NULL) {
        DestroyWindow(child);
        child = FindWindowEx(hWndMain,NULL,NULL,NULL);
    }
    // Resize
    RECT rcClient;
    GetClientRect(hWndMain,&rcClient);
    *w = rcClient.right - rcClient.left;
    *h = rcClient.bottom - rcClient.top;
    return (hWndMain);
}

/* 
 * hal_label
 */
void *hal_label (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *str)
{
    RECT rcLable = whereAmI(0,0,*w,*h);
    HWND hwndLabel;
    
    //code inside WINAPI WinMain() function
    hwndLabel = CreateWindow("STATIC",str,
                                WS_VISIBLE | WS_CHILD | SS_LEFT,
                                0,0,
                                120,CONTROL_HEIGH,
                                hWndMain,NULL,hInstance,NULL);
    SetWindowText(hwndLabel,str);
    resize(hwndLabel,w,h);
    
    return (void*)hwndLabel;
}

/* 
 * hal_input
 */
void *hal_input (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *hint,
                 bool multiline,
                 bool readonly)
{
    RECT rcClient = {0};
    HWND hEdit = NULL;
    GetClientRect(hWndMain,&rcClient);
    *w = (rcClient.right - rcClient.left) / 2;
    *h = CONTROL_HEIGH;
    rcClient = whereAmI(0,0,*w,*h);
    DWORD dwStyle = WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT;
    if (multiline) dwStyle |= ES_MULTILINE;
    if (readonly) dwStyle |= ES_READONLY;
    
    hEdit = CreateWindow("EDIT", hint,
                      dwStyle,
                      0, 0, 150, CONTROL_HEIGH,
                      hWndMain,
                      (HMENU)0, hInstance, NULL);
    resize(hEdit,w,h);
    return (void*)hEdit;
}

/* 
 * hal_button
 */
void *hal_button(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic,
                 const char *str, const char *img)
{
    RECT rcButton = whereAmI(0,0,*w,*h);
    Actionifier *act = actionifier_new(context,uictx,logic,NULL);
    HWND hButton = CreateWindow( "button", str,
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                0, 0, 
                300, CONTROL_HEIGH,
                hWndMain, (HMENU) BUTTON_ID,
                hInstance, NULL );
    SetWindowLong(hButton, GWLP_USERDATA, (LONG)act);
    resize(hButton,w,h);
    return (void*)hButton;
}

/* 
 * hal_table
 */
void *hal_table (struct context *context,
                 struct variable *uictx,
                 struct variable *list,
                 struct variable *logic)
{
    
    assert_message(list && ((list->type == VAR_LST) || (list->type == VAR_NIL)), "not a list");
    if (list->type == VAR_NIL)
        list = variable_new_list(context, NULL);
    
    // create list view
    Actionifier *act = actionifier_new(context,uictx,logic,list);
    HWND hListView = CreateListView(hWndMain,act);
    SetWindowLong(hListView, GWLP_USERDATA, (LONG)act);
    if (hListView == NULL) 
        DEBUGPRINT("%s\n","Listview not created!");         
    
    return (void*)hListView;
}

/* 
 * hal_ui_get
 */
struct variable *hal_ui_get(struct context *context, void *field)
{
    HWND hControl = (HWND)field;
    char szClassName[128] = {0};
    GetClassName(hControl,szClassName,128);
    
    if (strcmp(szClassName, "Edit") == 0) {
        char szText[128] = {0};
        GetWindowText(hControl,szText,sizeof(szText));
        return variable_new_str_chars(context, szText);        
    }
    return variable_new_nil(context);
}

/* 
 * hal_ui_set
 */
void hal_ui_set(void *field, struct variable *value)
{
    HWND hControl = (HWND)field;
    char szClassName[128] = {0};
    GetClassName(hControl,szClassName,128);
    if (strcmp(szClassName, "Edit") == 0) {
        const char *value2 = byte_array_to_string(value->str);
        SetWindowText(hControl,value2);
    }
    else if (strcmp(szClassName, "SysListView32") == 0) {
        Actionifier *act = (Actionifier *)GetWindowLong(hControl, GWLP_USERDATA);
        act->data = value;
        PostMessage(hControl,WMU_RELOAD_LISTVIEW,(LPARAM)act,0);
    }
}

/* 
 * hal_ui_put
 */
void hal_ui_put(void *field, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (field != NULL) {
        char szClassName[128] = {0};
        HWND hWnd = (HWND)field;
        GetClassName(hWnd,szClassName,128);
        if (strcmp(szClassName,"SysListView32") == 0) {
            RECT lsRect;
            int offset = 3 * CONTROL_HEIGH + 5;
            GetClientRect(hWnd,&lsRect);           
            MoveWindow(hWnd,lsRect.left,
                    lsRect.top + offset,
                    lsRect.right - lsRect.left, 
                    lsRect.bottom - lsRect.top - offset,
                    TRUE);
        }
        else {
            RECT rcClient = whereAmI(x,y,w,h);
            MoveWindow(hWnd, rcClient.left, rcClient.top,
                rcClient.right - rcClient.left,
                rcClient.bottom - rcClient.top,TRUE); 
        }
    }
}
#endif // NO_UI

int w_system(const char* command)
{
    char szComSpec[MAX_PATH + 1];
    DWORD dwLen = GetEnvironmentVariable(TEXT("COMSPEC"), szComSpec, MAX_PATH);
    
    if (dwLen == 0) {
        strcpy(szComSpec,"C:\\Windows\\system32\\cmd.exe");
        dwLen = strlen(szComSpec);
    }
    
    if ((dwLen == 0) || (dwLen > MAX_PATH))
        return -1;

    LPTSTR szCmdLine = (LPTSTR) LocalAlloc(LPTR, (dwLen+lstrlen(command)+9) * sizeof(char));
    if (!szCmdLine)
        return -1;

    sprintf(szCmdLine, TEXT("\"%s\" /C \"%s\""), szComSpec, command);

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL bRet = CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (bRet) {
        DWORD dwErr = 0;
        WaitForSingleObject( pi.hProcess, INFINITE );
        // Check whether our command succeeded?
        GetExitCodeProcess( pi.hProcess, &dwErr );
        // Avoid memory leak by closing process handle
        CloseHandle(pi.hThread);
        CloseHandle( pi.hProcess );
    }
    
    LocalFree(szCmdLine);

    return (bRet) ? 0 : -1;
}

// move file or folder
struct variable *sys_mv(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    const char *src = param_str(value, 1);
    const char *dst = param_str(value, 2);
    long timestamp = param_int(value, 3);

    assert_message((strlen(src)>1) && (strlen(dst)>1), "oops");

    char mvcmd[1024];
    sprintf(mvcmd, "mv \"%s\" \"%s\"", src, dst);
    if (w_system(mvcmd))
        printf("\nCould not mv from %s to %s\n", src, dst);

    if (timestamp) // to prevent unwanted timestamp updates resulting from the mv
        file_set_timestamp(dst, timestamp);

    return NULL;
}

// deletes file or folder
struct variable *sys_rm(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    char *path2 = byte_array_to_string(path->str);
    assert_message(strlen(path2)>1, "oops");
    char rmcmd[100];
    sprintf(rmcmd, "rm -rf \"%s\"", path2);
    if (w_system(rmcmd))
        DEBUGPRINT("\nCould not rm %s\n", path2);
    free(path2);
    return NULL;
}

// creates directory
struct variable *sys_mkdir(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);
    char *path2 = byte_array_to_string(path->str);
    char mkcmd[100];
    sprintf(mkcmd, "mkdir \"%s\"", path2);
    if (w_system(mkcmd))
        DEBUGPRINT("\nCould not mkdir %s\n", path2);

    free(path2);
    return NULL;
}

/****************************************************************************** 
 * WIN32 UI
 *****************************************************************************/

#ifndef NO_UI

/* 
 * OpenWithFile
 */
void OpenWithFile(const char *file)
{
    char *path = ExecuteAppendPath(file);
    ShellExecute(hWndMain, NULL, path, NULL, NULL, SW_SHOWNORMAL);
    DWORD error = GetLastError();
    if (error) {
        MessageBox(NULL, "Can't open the file",
               "Error!", MB_OK | MB_ICONERROR);
    }
    free (path);
}

/* 
 * ListViewColumnTitleAt
 */
CHAR *ListViewColumnTitleAt(int index, Actionifier *act)
{
    switch (index) {
        case 0:
            return TEXT("Item");            
        default:
            break;           
    }
}

/* 
 * ListViewNumberColumn
 */
int ListViewNumberColumn(Actionifier *act)
{
    return 1;
}

/* 
 * ListViewNumberRow
 */
int ListViewNumberRow(Actionifier *act)
{    
    return act->data->list.ordered->length;
}

/* 
 * ListViewValueAt
 */
LPARAM ListViewValueAtRow( int row, Actionifier *act)
{
    struct variable *item = array_get(act->data->list.ordered, (uint32_t)row);
    const char *name = variable_value_str(act->context, item);
    return (LPARAM)name;
}

/* 
 * ListViewSelectedAtRow
 */
void ListViewSelectedAtRow(int row, Actionifier *act)
{
#if 1
    return;
#elif 0    
    int32_t iRow = (int32_t)row;
    if (iRow == -1)
        return;
    act->param = variable_new_int(act->context, iRow);
    OnPress(NULL,act);
#else     
    OpenWithFile("C://Users//nqminh//PROJECTS//Filagree//source//file.c");
#endif    
}

/* 
 * ListViewSelectedAtRow
 */
void ListViewDoubleClickAtRow(int row, Actionifier *act)
{
#if 1
    int32_t iRow = (int32_t)row;
    if (iRow == -1)
        return;
    act->param = variable_new_int(act->context, iRow);
    OnPress(NULL,act);
#else     
    OpenWithFile("C://Users//nqminh//PROJECTS//Filagree//source//file.c");
#endif    
}

/* 
 * NotifyHandler
 */
LRESULT NotifyHandler( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LV_DISPINFO *pLvdi = (LV_DISPINFO *)lParam;
	NM_LISTVIEW *pNm = (NM_LISTVIEW *)lParam;
    
	char *pContext = (char *)(pLvdi->item.lParam);

	if (wParam != ID_LISTVIEW)
		return 0L;
    
    // if code == NM_CLICK - Single click on an item
    if(((LPNMHDR)lParam)->code == NM_DBLCLK) 
    {
        HWND hList = pNm->hdr.hwndFrom;
        Actionifier *act = (Actionifier *)GetWindowLong(hList, GWLP_USERDATA);
        ListViewDoubleClickAtRow( pNm->iItem, act);
        return 0L;
    }
    
	switch(pLvdi->hdr.code)
	{
		case LVN_GETDISPINFO:

			switch (pLvdi->item.iSubItem)
			{
				case 0:     // table's item name
					pLvdi->item.pszText = pContext;
					break;
				
				default:
					break;
			}
			break;

        case LVN_ITEMCHANGED :
        {
           if ((pNm->uChanged   & LVIF_STATE) &&
               (pNm->uNewState & LVIS_SELECTED) != 
               (pNm->uOldState & LVIS_SELECTED) ) {
               if (pNm->uNewState & LVIS_SELECTED) {
                   HWND hList = pNm->hdr.hwndFrom;
                   Actionifier *act = (Actionifier *)GetWindowLong(hList, GWLP_USERDATA);
                   ListViewSelectedAtRow( pNm->iItem, act);
               }
               else {
                   // Deselected
               }        
            }
            break;
        }
        case LVN_BEGINLABELEDIT:
            break;
        case LVN_ENDLABELEDIT:
            break;
        case LVN_COLUMNCLICK:			
             break;
        default:
             break;
	}
	return 0L;
}

/* 
 * ReloadListView
 */
void ReloadListView(HWND hList,Actionifier *act)
{    	
	RECT rcl;                       // Rectangle for setting the size of the window
	int index;                      // Index used in for loops
	LV_COLUMN lvC;                  // List View Column structure
	LV_ITEM lvI;                    // List view item structure
	int iSubItem;       
    SendMessage(hList,LVM_DELETEALLITEMS,0,0);
    	// Initialize the columns
	lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.fmt = LVCFMT_CENTER;
	lvC.cx = rcl.right - rcl.left - 2;

	// Columns.
    int nColumn = ListViewNumberColumn(act);
    
	for (index = 0; index < nColumn; index++) {
		lvC.iSubItem = index;
        lvC.pszText = ListViewColumnTitleAt(index,act); 
		if (ListView_InsertColumn(hList, index, &lvC) == -1)
			return ;
	}

	// Finally, let's add the actual items to the control
	lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
	lvI.state = 0;      //
	lvI.stateMask = 0;  //
        
    //Rows
    int nRows = ListViewNumberRow(act);
	for (index = 0; index < nRows; index++) {
		lvI.iItem = index;
		lvI.iSubItem = 0;
		lvI.pszText = LPSTR_TEXTCALLBACK; 
		lvI.cchTextMax = MAX_ITEMLEN;
		lvI.iImage = index;
        lvI.lParam = (LPARAM)ListViewValueAtRow(index,act);
            
        //if (SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lvI) == -1)
		if (ListView_InsertItem(hList, &lvI) == -1)
			return ;

		for (iSubItem = 1; iSubItem < nColumn; iSubItem++) {
			ListView_SetItemText( hList,
				index,
				iSubItem,
				LPSTR_TEXTCALLBACK);
		}
	}
    
    InvalidateRect(hList,NULL,TRUE);
}

/* 
 * CreateListView
 */
HWND CreateListView (HWND hWndParent, Actionifier *act)                                     
{
	HWND hWndList;                  // Handle to the list view window
	RECT rcl;                       // Rectangle for setting the size of the window

	// Get the size and position of the parent window
	GetClientRect(hWndParent, &rcl);

	// Create the list view window that starts out in report view and allows label editing.
	hWndList = CreateWindowEx( 0L,
		WC_LISTVIEW,                // list view class
		"Listview",                         // no default text
		WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT |
		LVS_EDITLABELS | WS_EX_CLIENTEDGE,
		0, 0,
		rcl.right - rcl.left, rcl.bottom - rcl.top,
		hWndParent,
		(HMENU) ID_LISTVIEW,
		hInstance,
		NULL );
        
	if (hWndList == NULL )
		return NULL;

    SendMessage(hWndList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT);
    PostMessage(hWndList,WMU_RELOAD_LISTVIEW,(LPARAM)act,0);  
	return (hWndList);
}

/* 
 * ActCallback
 */
void ActCallback(Actionifier *act)
{
    if (act->logic && act->logic->type != VAR_NIL)
    {
        gil_lock(act->context, "pressed");
        vm_call(act->context, act->logic, act->uictx, act->param, NULL);
        gil_unlock(act->context, "pressed");
    }
}

/* 
 * ShowDetailWindow
 */
void ShowDetailWindow(const char *file)
{
    HWND hOpenWnd = CreateWindowEx( 0L,
		 TEXT("Filagree Windows"),            // list view class
		 "Show file",                         // no default text
		 WS_POPUP,
	  	 0, 0,
		 0, 0,
		 hWndMain,
		 (HMENU) 0x0,
		 hInstance,
		 NULL );
    DWORD style = GetWindowLong(hWndMain,GWL_STYLE);
    style &= ~(WS_POPUP);
    style |= WS_CHILD;
    SetWindowLong(hOpenWnd,GWL_STYLE,style);
    RECT rc; 
    GetWindowRect(hWndMain,&rc);
    MoveWindow(hOpenWnd,rc.left,rc.top + 20,(rc.right - rc.left),(rc.bottom - rc.top) - 40,TRUE);
    UpdateWindow(hWndMain);
}

/* 
 * OnPress
 */
void OnPress(HWND hWnd, Actionifier *act)
{
#if 0   
    ShowDetailWindow();
#endif    
    ActCallback(act);
}

/* 
 * MainWndProc
 */
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{     
  switch (msg)
  {
    case WM_CREATE:
        break;
    case WM_NOTIFY:
        return( NotifyHandler(hWnd, msg, wParam, lParam));
        break;
    case WM_COMMAND:
    {
        switch(LOWORD(wParam))
        {
            case BUTTON_ID:
            {
                HWND hControl = (HWND)lParam;
                Actionifier *act = (Actionifier *)GetWindowLong(hControl, GWLP_USERDATA);
                OnPress(hControl,act);
                break;
            }
        }
        break;
    }
         
    case WM_DESTROY:
        if (hWnd == hWndMain)
            PostQuitMessage(0);
        break;
  }
  
  return DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif

/* 
 * Execute path. End of string with '\\'
 */
const char *ExecutePath()
{
    static char szPath[1024] = {0};
    if (strlen(szPath) == 0) {
        GetModuleFileName(NULL, szPath, 512);
        char * pch = 0;int ls = 0;
        pch = strrchr(szPath,'\\');
        ls = pch - szPath + 1;
        szPath[ls] = 0;
        szPath[ls + 1] = 0;
    }
    return &szPath[0];
}

/* 
 * ExecuteAppendPath
 */
char *ExecuteAppendPath(const char *stringAppend)
{
    const char *lpszExecutePath = ExecutePath();
    int len = strlen(lpszExecutePath) + strlen(stringAppend);
    char *str = (char*)malloc(len);
    strcpy(str,lpszExecutePath);
    strcat(str,stringAppend);
    return str;
}
/* 
 * Bytes from resource file
 */
struct byte_array * ReadbytesFromResource(const char* resourceFile)
{
    char *scriptPath = ExecuteAppendPath(resourceFile);
    struct byte_array *filename = byte_array_from_string(scriptPath);
    struct byte_array *bytereads = read_file(filename,0,0);
    byte_array_del(filename);
    free(scriptPath);
    return bytereads;
}

/* 
 * AttachFilagree
 */
void AttachFilagree()
{
#if 0
    struct byte_array *script = byte_array_from_string("sys.print('hi')");
#elif 0
    struct byte_array *script = ReadbytesFromResource("..\\ui.fg");
#else
    struct byte_array *ui = ReadbytesFromResource("..\\ui.fg");
    struct byte_array *sync = ReadbytesFromResource("sync.fg");
    struct byte_array *mesh = ReadbytesFromResource("..\\mesh.fg");
    struct byte_array *im_client = ReadbytesFromResource("sync_client.fg");
    struct byte_array *script = byte_array_concatenate(4, ui, mesh, sync, im_client);
#endif
    struct byte_array *program = build_string(script);
    execute(program);
}

#ifndef NO_UI
/* 
 * WIN32 MAIN
 */
int WINAPI WinMain(HINSTANCE hxInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{ 
    LPCTSTR MainWndClass = TEXT("Filagree Windows");

    WNDCLASSEX wc    = {0};  // make sure all the members are zero-ed out to start
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hxInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = MainWndClass;

    if (!RegisterClassEx(&wc)) {       
        MessageBox(NULL, "Class registration has failed!",
               "Error!", MB_OK | MB_ICONERROR);
        return 0;
    }
    
    // Create window
    DWORD csStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    hWndMain = CreateWindowEx(0,MainWndClass,
                              TEXT("Filagree UI"),
                              csStyle,
                              CW_USEDEFAULT,
                              0,
                              CW_USEDEFAULT,
                              0, NULL, NULL,
                              hxInstance,
                              NULL);
    hInstance = hxInstance;
    // Error if window creation failed.
    if (!hWndMain) {
        DWORD error = GetLastError();
        MessageBox(NULL, TEXT("Error creating main window."), TEXT("Error"), MB_ICONERROR | MB_OK);
        return 0;
    }
    WMU_RELOAD_LISTVIEW = RegisterWindowMessage("WMU_RELOAD_LISTVIEW");
    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);
    InitCommonControls();
  
#if 0
  struct byte_array *script = byte_array_from_string("sys.print('hi')");
  struct byte_array *program = build_string(script);
  execute(script);
#else    
  AttachFilagree();
#endif    
  
    MSG msg = {0};
    HACCEL hAccelTable = NULL;
    // Main message loop.
    while(GetMessage(&msg, NULL, 0, 0) > 0){        
        if (msg.message == WMU_RELOAD_LISTVIEW) {
            HWND hControl = (HWND)msg.hwnd;
            Actionifier *act = (Actionifier*)msg.wParam;
            ReloadListView(hControl,act); 
        } 
        if (! TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
    }
    return (int) 0;
}
#endif