#include "wasapi_render.h"

#include <thread>
#include "common_logger.h"
#include "wasapi_utils.h"

namespace
{
#define CLOSE_HANDLE(punk)  \
			if((punk) && (punk) != INVALID_HANDLE_VALUE) \
			{ \
				CloseHandle((punk)); \
				(punk) = NULL; \
			}

#define SAFE_RELEASE(punk)  \
			if ((punk) != NULL)  \
			{ (punk)->Release(); (punk) = NULL; }
}

WASAPIRender::WASAPIRender(std::map<std::string, std::string>& settings)
	:WASAPIDevice()
{
	m_enumerator = NULL;
	m_mmdevice = NULL;
	m_client = NULL;
	m_render = NULL;
	m_notify = NULL;

	m_is_default_device = false;
	m_is_playing = false;
	m_is_reconnecting = false;

	m_sample_rate = 0;
	m_sample_channels = 0;
	m_wasapi_format = WASAPIFormat::Unknown;
	m_last_notify_time = 0;
	m_block_align = 0;

	m_buffer_frame_count = 0;

	m_stop_event = NULL;
	m_reconnect_thread_handle = NULL;

	update_settings(settings);
}

WASAPIRender::~WASAPIRender()
{
	stop_render();
	free_resources();

	CLOSE_HANDLE(m_stop_event);
	CLOSE_HANDLE(m_reconnect_thread_handle);
}

void WASAPIRender::free_resources()
{
	if (m_enumerator && m_notify)
	{
		m_enumerator->UnregisterEndpointNotificationCallback(m_notify);
	}

	SAFE_RELEASE(m_notify);
	SAFE_RELEASE(m_render);
	SAFE_RELEASE(m_client);
	SAFE_RELEASE(m_mmdevice);
	SAFE_RELEASE(m_enumerator);
}

bool WASAPIRender::initialize()
{
	m_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!m_stop_event)
	{
		LOG_ERROR("CreateEvent has error");
		return false;
	}

	return true;
}

void WASAPIRender::update(std::map<std::string, std::string>& settings)
{
	std::string newDeviceID = settings["device_id"];
	bool restart = (newDeviceID != m_device_id);

	update_settings(settings);
	if (restart && m_is_playing)
	{
		stop_render();
		free_resources();
		start_render();
	}
}

void WASAPIRender::update_settings(std::map<std::string, std::string>& settings)
{
	m_device_id = settings["device_id"];
	m_is_default_device = (m_device_id == "default");

	m_sample_rate = (uint32_t)std::stoi(settings["sample_rate"]);
	m_sample_channels = (uint32_t)std::stoi(settings["channels"]);

	std::string fmt = settings["wasapi_format"];
	if (fmt == "WASAPI_F32SYS")
	{
		m_wasapi_format = WASAPIFormat::WASAPI_F32SYS;
	}
	else if (fmt == "WASAPI_S16SYS")
	{
		m_wasapi_format = WASAPIFormat::WASAPI_S16SYS;
	}
	else if (fmt == "WASAPI_S32SYS")
	{
		m_wasapi_format = WASAPIFormat::WASAPI_S32SYS;
	}
	else
	{
		m_wasapi_format = WASAPIFormat::Unknown;
	}
}

void WASAPIRender::set_default_device(EDataFlow flow, ERole role, LPCWSTR id)
{
	if (!m_is_default_device)
	{
		return;
	}

	EDataFlow expectedFlow = eRender;
	ERole expectedRole = eConsole;

	if (flow != expectedFlow || role != expectedRole)
	{
		return;
	}

	//the same device
	if (id && m_default_id.compare(id) == 0)
	{
		return;
	}

	// reset device in 500ms intervals
	uint64_t t = gettime_in_ns();
	if (t - m_last_notify_time < 500000000)
	{
		return;
	}

	if (m_is_playing)
	{
		std::thread([this]()
		{
			stop_render();
			free_resources();
			start_render();
		}).detach();
	}

	m_last_notify_time = t;
}

void WASAPIRender::start_render()
{
	if (!try_initialize())
	{
		reconnect();
	}
}

void WASAPIRender::stop_render()
{
	if (m_stop_event)
	{
		SetEvent(m_stop_event);

		if (m_is_reconnecting)
		{
			WaitForSingleObject(m_reconnect_thread_handle, INFINITE);
			m_is_reconnecting = false;
		}

		ResetEvent(m_stop_event);

		if (m_client)
		{
			m_client->Stop();
		}

		m_is_playing = false;
	}
}

bool WASAPIRender::try_initialize()
{
	return inner_initialize();
}

bool WASAPIRender::inner_initialize()
{
	HRESULT hr;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&m_enumerator);
	if (FAILED(hr))
	{
		goto exitFlag;
	}

	if (!initialize_device())
	{
		goto exitFlag;
	}

	m_device_name = GetWASDeviceName(m_mmdevice);

	m_notify = new (std::nothrow)WASAPINotify(this);
	if (!m_notify)
	{
		LOG_ERROR("Out of memory");
		goto exitFlag;
	}

	m_enumerator->RegisterEndpointNotificationCallback(m_notify);

	if (!initialize_client())
	{
		goto exitFlag;
	}

	if (!initialize_render())
	{
		goto exitFlag;
	}

	return true;

exitFlag:
	free_resources();
	return false;
}

bool WASAPIRender::initialize_device()
{
	HRESULT hr;

	if (m_is_default_device)
	{
		hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eCommunications, &m_mmdevice);
		if (FAILED(hr))
		{
			return false;
		}

		wchar_t* id;
		hr = m_mmdevice->GetId(&id);
		m_default_id = id;

		CoTaskMemFree(id);
	}
	else
	{
		int wcsLen = MultiByteToWideChar(CP_UTF8, NULL, m_device_id.c_str(), m_device_id.size(), NULL, 0);
		wchar_t* w_id = new (std::nothrow) wchar_t[wcsLen + 1];
		if (!w_id)
		{
			return false;
		}

		MultiByteToWideChar(CP_UTF8, NULL, m_device_id.c_str(), m_device_id.size(), w_id, wcsLen);
		w_id[wcsLen] = '\0';

		hr = m_enumerator->GetDevice(w_id, &m_mmdevice);

		delete[] w_id;
	}

	return SUCCEEDED(hr);
}

bool WASAPIRender::initialize_client()
{
	WAVEFORMATEX* wfex = NULL;
	WAVEFORMATEX* pClosestMatch = NULL;
	HRESULT hr;

	hr = m_mmdevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
		(void **)&m_client);
	if (FAILED(hr))
	{
		return false;
	}

	hr = m_client->GetMixFormat(&wfex);
	if (FAILED(hr))
	{
		return false;
	}
	
	if (!reset_format_from(wfex, this->m_sample_rate, this->m_sample_channels, this->m_wasapi_format))
	{
		CoTaskMemFree(wfex);
		return false;
	}

	//hr = m_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, wfex, &pClosestMatch);
	hr = S_OK;
	if (hr == S_OK)
	{
		//buffer that can hold at least 2 seconds audio data
		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
			2 * 10000000, 0, wfex, NULL);
		m_block_align = wfex->nBlockAlign;
	}
	else if (hr == S_FALSE)
	{
		//buffer that can hold at least 2 seconds audio data
		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
			2 * 10000000, 0, pClosestMatch, NULL);
		m_block_align = pClosestMatch->nBlockAlign;
	}
	else
	{
		m_block_align = 0;
		CoTaskMemFree(wfex);
		return false;
	}

	CoTaskMemFree(wfex);
	if (pClosestMatch)
	{
		CoTaskMemFree(pClosestMatch);
		pClosestMatch = NULL;
	}
	if (FAILED(hr))
	{
		LOG_ERROR("WASAPIRender initialize failed");
		return false;
	}

	return true;
}

bool WASAPIRender::initialize_render()
{
	HRESULT hr;

	// get the actual size of the allocated buffer.
	hr = m_client->GetBufferSize(&m_buffer_frame_count);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = m_client->GetService(__uuidof(IAudioRenderClient), (void **)&m_render);
	if (FAILED(hr))
	{
		goto errorFlag;
	}
	
	hr = m_client->Start();
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	m_is_playing = true;
	return true;

errorFlag:
	return false;
}

bool WASAPIRender::reconnect()
{
	m_is_reconnecting = true;

	free_resources();
	m_reconnect_thread_handle = CreateThread(NULL, 0, WASAPIRender::reconnect_thread, this, 0, NULL);
	if (!m_reconnect_thread_handle)
	{
		m_is_reconnecting = false;
		return false;
	}

	return true;
}

DWORD WINAPI WASAPIRender::reconnect_thread(LPVOID param)
{
	WASAPIRender *render = (WASAPIRender *)param;

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	bool com_initialized = SUCCEEDED(hr);
	if (!com_initialized)
	{
		LOG_ERROR("CoInitializeEx error");
	}

	while (WaitForSingleObject(render->m_stop_event, 3000) == WAIT_TIMEOUT)
	{
		if (render->try_initialize())
		{
			break;
		}
	}

	if (com_initialized)
	{
		CoUninitialize();
	}

	CloseHandle(render->m_reconnect_thread_handle);
	render->m_reconnect_thread_handle = NULL;
	render->m_is_reconnecting = false;
	
	return 0;
}


bool WASAPIRender::render_data(uint8_t* buffer, int len)
{
	if (m_is_reconnecting)
	{
		return false;
	}

	HRESULT hr;
	UINT32 numFramesPadding;
	UINT32 numFramesAvailable;
	UINT32 numAppropriateFrames;
	BYTE *pData;

	hr = m_client->GetCurrentPadding(&numFramesPadding);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	numFramesAvailable = m_buffer_frame_count - numFramesPadding;
	if (len <= numFramesAvailable * m_block_align)
	{
		numAppropriateFrames = len / m_block_align;
		if (len % m_block_align != 0)
		{
			numAppropriateFrames++;
		}
	}
	else
	{
		LOG_WARNING("Data is too big");
		return false;
	}

	// grab the appropriate space in the shared buffer.
	hr = m_render->GetBuffer(numAppropriateFrames, &pData);
	if (FAILED(hr))
	{
		goto errorFlag;
	}
	
	memcpy(pData, buffer, len);

	hr = m_render->ReleaseBuffer(numAppropriateFrames, 0);
	return true;

errorFlag:

	m_is_playing = false;
	this->reconnect();

	return false;
}