
// mmcaptureropengl.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "mmcaptureropengl.h"
#include "mmcaptureropenglDlg.h"

#include "common_logger.h"
#include "common_utils.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

AppLogger *g_pLogger = NULL;
int g_log_level = LOG_LEVEL_VERBOSE;

// CmmcaptureropenglApp

BEGIN_MESSAGE_MAP(CmmcaptureropenglApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CmmcaptureropenglApp 构造

CmmcaptureropenglApp::CmmcaptureropenglApp()
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CmmcaptureropenglApp 对象

CmmcaptureropenglApp theApp;


// CmmcaptureropenglApp 初始化

BOOL CmmcaptureropenglApp::InitInstance()
{
	CWinApp::InitInstance();


	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	WORD versionRequested;
	WSADATA wsaData;
	int err;

	versionRequested = MAKEWORD(2, 2);
	err = WSAStartup(versionRequested, &wsaData);
	if (err != 0)
	{
		// we could not find a usable Winsock DLL.
		return FALSE;
	}

	/*
	* Confirm that the WinSock DLL supports 2.2.
	* Note that if the DLL supports versions greater than 2.2 in addition to 2.2,
	* it will still return  2.2 in wVersion since that is the version we requested.
	*/
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		// we could not find a usable Winsock DLL.
		WSACleanup();
		return FALSE;
	}

	if (!is_file_exist(_T("D:\\mmcaptureropengl\\logs")))
	{
		mkdir_recursively("D:\\mmcaptureropengl\\logs");
	}

	AppLogger *pLogger = new AppLogger("mmcaptureropengl");
	if (pLogger)
	{
		bool ret = pLogger->initialize("D:\\mmcaptureropengl\\logs", true, 1024 * 10, 10);
		std::cout << "AppLogger::initialize(): " << ret << std::endl;
	}
	g_pLogger = pLogger;

	LOG_INFO("Program begins.......");

	CmmcaptureropenglDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	LOG_INFO("Program ends.......");
	if (g_pLogger)
	{
		g_pLogger->uninitialize();
		delete g_pLogger;
	}
	WSACleanup();

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

