#include "wasapi_capturer.h"

#include <thread>
#include <sstream>
#include "common_logger.h"
#include "wasapi_utils.h"

#define CLOSE_HANDLE(punk)  \
			if((punk) && (punk) != INVALID_HANDLE_VALUE) \
			{ \
				CloseHandle((punk)); \
				(punk) = NULL; \
			}

#define SAFE_RELEASE(punk)  \
			if ((punk) != NULL)  \
			{ (punk)->Release(); (punk) = NULL; }


WASAPICapturer::WASAPICapturer(bool inputDevice)
	:WASAPIDevice()
{
	m_is_input_device = inputDevice;

	m_enumerator = NULL;
	m_mmdevice = NULL;
	m_client = NULL;
	m_capture = NULL;
	m_render = NULL;
	m_notify = NULL;

	m_use_device_timing = false;
	m_is_default_device = false;
	m_is_capturing = false;
	m_is_reconnecting = false;

	m_sample_rate = 0;
	m_sample_channels = 0;
	m_last_notify_time = 0;

	m_stop_event = NULL;

	m_capture_thread_handle = NULL;
	m_reconnect_thread_handle = NULL;

	m_num_buffer_frames = 0;
}

WASAPICapturer::~WASAPICapturer()
{
	stop_capture();
	free_resources();
	CLOSE_HANDLE(m_stop_event)
	CLOSE_HANDLE(m_capture_thread_handle)
	CLOSE_HANDLE(m_reconnect_thread_handle)
}

void WASAPICapturer::free_resources()
{
	if (m_enumerator && m_notify)
	{
		m_enumerator->UnregisterEndpointNotificationCallback(m_notify);
	}

	if (m_client)
	{
		m_client->SetEventHandle(NULL);
	}
	SAFE_RELEASE(m_notify)
	SAFE_RELEASE(m_capture)
	SAFE_RELEASE(m_render)
	SAFE_RELEASE(m_client)
	SAFE_RELEASE(m_mmdevice)
	SAFE_RELEASE(m_enumerator)
}

void WASAPICapturer::set_sample_callback(WASAPISampleCallback sampleCallback)
{
	this->m_sample_callback = sampleCallback;
}

bool WASAPICapturer::initialize()
{
	m_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!m_stop_event)
	{
		LOG_ERROR("CreateEvent has error");
		return false;
	}

	return true;
}

void WASAPICapturer::update(std::map<std::string, std::string>& settings)
{
	std::string newDeviceID = settings["device_id"];
	bool restart = (newDeviceID != m_device_id);

	update_settings(settings);

	if (restart && m_is_capturing)
	{
		stop_capture();
		free_resources();
		start_capture();
	}
}

void WASAPICapturer::update_settings(std::map<std::string, std::string>& settings)
{
	m_device_id = settings["device_id"];
	std::istringstream(settings["use_device_timing"]) >> std::boolalpha >> m_use_device_timing;
	m_is_default_device = (m_device_id == "default");
}

void WASAPICapturer::set_default_device(EDataFlow flow, ERole role, LPCWSTR id)
{
	if (!m_is_default_device)
	{
		return;
	}

	EDataFlow expectedFlow = m_is_input_device ? eCapture : eRender;
	ERole expectedRole = m_is_input_device ? eCommunications : eConsole;

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

	if (m_is_capturing)
	{
		std::thread([this]()
		{
			stop_capture();
			free_resources();
			start_capture();
		}).detach();
	}

	m_last_notify_time = t;
}

void WASAPICapturer::start_capture()
{
	if (!try_initialize())
	{
		reconnect();
	}
}

void WASAPICapturer::stop_capture()
{
	if (m_stop_event)
	{
		SetEvent(m_stop_event);
		if (m_is_capturing)
		{
			WaitForSingleObject(m_capture_thread_handle, INFINITE);
			m_is_capturing = false;
		}
		if (m_is_reconnecting)
		{
			WaitForSingleObject(m_reconnect_thread_handle, INFINITE);
			m_is_reconnecting = false;
		}
		ResetEvent(m_stop_event);
	}
}

bool WASAPICapturer::try_initialize()
{
	return inner_initialize();
}

bool WASAPICapturer::inner_initialize()
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

	if (!m_is_input_device)
	{
		initialize_render();
	}

	if (initialize_capture())
	{
		return true;
	}

exitFlag:
	free_resources();
	return false;
}

bool WASAPICapturer::initialize_device()
{
	HRESULT hr;

	if (m_is_default_device)
	{
		hr = m_enumerator->GetDefaultAudioEndpoint(
			m_is_input_device ? eCapture : eRender,
			m_is_input_device ? eCommunications : eConsole, 
			&m_mmdevice);
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

bool WASAPICapturer::initialize_client()
{
	WAVEFORMATEX* wfex;
	WAVEFORMATEX* pClosestMatch;
	HRESULT hr;
	DWORD flags = 0;

	if (!m_is_input_device)
	{
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
	}

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

	if (wfex->nChannels > 2)
	{
		wfex->nChannels = 2;
	}
	(void)reset_format_to_16bits(wfex);

	hr = m_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, wfex, &pClosestMatch);
	if(hr == S_OK)
	{ 
		//buffer that can hold at least 1 seconds audio data
		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
			1 * 10000000, 0, wfex, NULL);

		this->m_sample_rate = (uint32_t)wfex->nSamplesPerSec;
		this->m_sample_channels = (uint32_t)wfex->nChannels;
		this->m_wasapi_format = waveformat_to_wasapiformat(wfex);
	}
	else if (hr == S_FALSE)
	{
		//buffer that can hold at least 1 seconds audio data
		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
			1 * 10000000, 0, pClosestMatch, NULL);

		this->m_sample_rate = (uint32_t)pClosestMatch->nSamplesPerSec;
		this->m_sample_channels = (uint32_t)pClosestMatch->nChannels;
		this->m_wasapi_format = waveformat_to_wasapiformat(pClosestMatch);
	}
	else
	{
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
		return false;
	}

	//the number of audio frames that the buffer can hold.
	hr = m_client->GetBufferSize(&m_num_buffer_frames);
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool WASAPICapturer::initialize_capture()
{
	HRESULT hr = m_client->GetService(__uuidof(IAudioCaptureClient), (void **)&m_capture);
	if (FAILED(hr))
	{
		return false;
	}

	m_capture_thread_handle = CreateThread(NULL, 0, WASAPICapturer::capture_thread,
		this, 0, NULL);
	if (!m_capture_thread_handle)
	{
		return false;
	}

	hr = m_client->Start();
	if (FAILED(hr))
	{
		return false;
	}
	m_is_capturing = true;

	return true;
}

bool WASAPICapturer::initialize_render()
{
	HRESULT hr;
	BYTE* buffer;
	UINT32 numBufferFrames;
	WAVEFORMATEX* wfex = NULL;
	IAudioClient* client = NULL;

	hr = m_mmdevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
		(void **)&client);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = client->GetMixFormat(&wfex);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	//1 seconds
	hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1 * 10000000,
		0, wfex, NULL);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = client->GetBufferSize(&numBufferFrames);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = client->GetService(__uuidof(IAudioRenderClient), (void **)&m_render);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = m_render->GetBuffer(numBufferFrames, &buffer);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	memset(buffer, 0, numBufferFrames * wfex->nBlockAlign);
	m_render->ReleaseBuffer(numBufferFrames, 0);

	CoTaskMemFree(wfex);
	return true;

errorFlag:
	if (wfex)
	{
		CoTaskMemFree(wfex);
	}

	SAFE_RELEASE(client);

	return false;
}

bool WASAPICapturer::reconnect()
{
	free_resources();
	m_is_reconnecting = true;
	m_reconnect_thread_handle = CreateThread(NULL, 0, WASAPICapturer::reconnect_thread, this, 0, NULL);
	if (!m_reconnect_thread_handle)
	{
		m_is_reconnecting = false;
		return false;
	}

	return true;
}

DWORD WINAPI WASAPICapturer::reconnect_thread(LPVOID param)
{
	WASAPICapturer *capture = (WASAPICapturer *)param;

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	bool com_initialized = SUCCEEDED(hr);
	if (!com_initialized)
	{
		LOG_ERROR("CoInitializeEx error");
	}

	while (WaitForSingleObject(capture->m_stop_event, 3000) == WAIT_TIMEOUT)
	{
		if (capture->try_initialize())
		{
			break;
		}
	}

	if (com_initialized)
	{
		CoUninitialize();
	}

	capture->m_is_reconnecting = false;

	CloseHandle(capture->m_reconnect_thread_handle);
	capture->m_reconnect_thread_handle = NULL;
	
	return 0;
}

DWORD WINAPI WASAPICapturer::capture_thread(LPVOID param)
{
	WASAPICapturer *capture = (WASAPICapturer *)param;
	
	capture->set_thread_characteristics();

	bool reconnect = false;
	DWORD ret = WaitForSingleObject(capture->m_stop_event, 25);
	while (ret == WAIT_TIMEOUT)
	{
		if (!capture->process_capture_data())
		{
			reconnect = true;
			break;
		}

		ret = WaitForSingleObject(capture->m_stop_event, 25);
	}

	capture->m_client->Stop();

	CloseHandle(capture->m_capture_thread_handle);
	capture->m_capture_thread_handle = NULL;
	capture->m_is_capturing = false;
	capture->revert_thread_characteristics();

	if (reconnect)
	{
		capture->reconnect();
	}

	return 0;
}

bool WASAPICapturer::process_capture_data()
{
	HRESULT hr;
	BYTE* buffer;
	UINT32 numFramesAvailable;
	DWORD flags;
	UINT64 pos;
	UINT64 ts;
	UINT numFramesInNextPacket = 0;

	hr = m_capture->GetNextPacketSize(&numFramesInNextPacket);
	if (FAILED(hr))
	{
		return false;
	}

	while (numFramesInNextPacket != 0)
	{
		hr = m_capture->GetBuffer(&buffer, &numFramesAvailable, &flags, &pos, &ts);
		if (FAILED(hr))
		{
			return false;
		}

		if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
		{
			m_sample.buffer = NULL;
		}
		else
		{
			m_sample.buffer = (uint8_t *)buffer;
		}
		m_sample.frames = (uint32_t)numFramesAvailable;

		uint64_t timestamp = m_use_device_timing ? ts * 100 : gettime_in_ns();
		if (!m_use_device_timing)
		{
			timestamp -= mul_div64(numFramesAvailable, 1000000000ULL, m_sample_rate);
		}
		m_sample.timestamp = timestamp;
		m_sample.channels = this->m_sample_channels;
		m_sample.format = this->m_wasapi_format;
		m_sample.sampleRate = this->m_sample_rate;

		m_sample_callback(&m_sample);
		m_capture->ReleaseBuffer(numFramesAvailable);

		hr = m_capture->GetNextPacketSize(&numFramesInNextPacket);
		if (FAILED(hr))
		{
			return false;
		}
	}

	return true;
}