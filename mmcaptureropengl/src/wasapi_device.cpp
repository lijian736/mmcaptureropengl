#include "wasapi_device.h"

#include <atomic>

namespace
{
	std::atomic<int> g_id_generator(1);
}

WASAPIDevice::WASAPIDevice()
{
	m_id = ++g_id_generator;
	m_task_handle = NULL;
}

WASAPIDevice::~WASAPIDevice()
{
}

int WASAPIDevice::get_id()
{
	return this->m_id;
}

void WASAPIDevice::set_thread_characteristics()
{
	if (MY_AvSetMmThreadCharacteristicsA)
	{
		DWORD idx = 0;
		this->m_task_handle = MY_AvSetMmThreadCharacteristicsA("Pro Audio", &idx);
	}
}

void WASAPIDevice::revert_thread_characteristics()
{
	if (this->m_task_handle && MY_AvRevertMmThreadCharacteristics)
	{
		MY_AvRevertMmThreadCharacteristics(this->m_task_handle);
		this->m_task_handle = NULL;
	}
}