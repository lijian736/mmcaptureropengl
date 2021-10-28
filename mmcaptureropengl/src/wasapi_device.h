#ifndef _H_WASAPI_DEVICE_H_
#define _H_WASAPI_DEVICE_H_

#include <audioclient.h>
#include <mmdeviceapi.h>

#include "wasapi_notify.h"
#include "wasapi_utils.h"

//the Windows Audio Session API device
class WASAPIDevice
{
public:
	WASAPIDevice();
	virtual ~WASAPIDevice();

	int get_id();
	virtual void set_default_device(EDataFlow flow, ERole role, LPCWSTR id) = 0;

	void set_thread_characteristics();
	void revert_thread_characteristics();
protected:
	//the id of the wasapi device
	int m_id;
	HANDLE m_task_handle;
};

#endif