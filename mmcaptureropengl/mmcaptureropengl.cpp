
// mmcaptureropengl.cpp : ����Ӧ�ó��������Ϊ��
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


// CmmcaptureropenglApp ����

CmmcaptureropenglApp::CmmcaptureropenglApp()
{
	// TODO: �ڴ˴���ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
}


// Ψһ��һ�� CmmcaptureropenglApp ����

CmmcaptureropenglApp theApp;


// CmmcaptureropenglApp ��ʼ��

BOOL CmmcaptureropenglApp::InitInstance()
{
	CWinApp::InitInstance();


	// ���� shell ���������Է��Ի������
	// �κ� shell ����ͼ�ؼ��� shell �б���ͼ�ؼ���
	CShellManager *pShellManager = new CShellManager;

	// ���Windows Native���Ӿ����������Ա��� MFC �ؼ�����������
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// ��׼��ʼ��
	// ���δʹ����Щ���ܲ�ϣ����С
	// ���տ�ִ���ļ��Ĵ�С����Ӧ�Ƴ�����
	// ����Ҫ���ض���ʼ������
	// �������ڴ洢���õ�ע�����
	// TODO: Ӧ�ʵ��޸ĸ��ַ�����
	// �����޸�Ϊ��˾����֯��
	SetRegistryKey(_T("Ӧ�ó��������ɵı���Ӧ�ó���"));

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
		// TODO: �ڴ˷��ô����ʱ��
		//  ��ȷ�������رնԻ���Ĵ���
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: �ڴ˷��ô����ʱ��
		//  ��ȡ�������رնԻ���Ĵ���
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "����: �Ի��򴴽�ʧ�ܣ�Ӧ�ó���������ֹ��\n");
		TRACE(traceAppMsg, 0, "����: ������ڶԻ�����ʹ�� MFC �ؼ������޷� #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS��\n");
	}

	// ɾ�����洴���� shell ��������
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

	// ���ڶԻ����ѹرգ����Խ����� FALSE �Ա��˳�Ӧ�ó���
	//  ����������Ӧ�ó������Ϣ�á�
	return FALSE;
}

