
// mmcaptureropengl.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CmmcaptureropenglApp: 
// �йش����ʵ�֣������ mmcaptureropengl.cpp
//

class CmmcaptureropenglApp : public CWinApp
{
public:
	CmmcaptureropenglApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CmmcaptureropenglApp theApp;