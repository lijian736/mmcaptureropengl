#include "wasapi_utils.h"

#include <VersionHelpers.h>
#include "common_utf8.h"

namespace
{
	HMODULE g_libAVRT = NULL;

	const PROPERTYKEY MY_PKEY_Device_FriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd,{ 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, } }, 14 };
	const PROPERTYKEY MY_PKEY_AudioEngine_DeviceFormat = { { 0xf19f064d, 0x82c, 0x4e27,{ 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c, } }, 0 };
}

pf_AvSetMmThreadCharacteristicsA MY_AvSetMmThreadCharacteristicsA = NULL;
pf_AvRevertMmThreadCharacteristics MY_AvRevertMmThreadCharacteristics = NULL;

static LONGLONG get_clock_freq()
{
	static bool have_clock_freq = false;
	static LARGE_INTEGER clock_freq;

	if (!have_clock_freq)
	{
		QueryPerformanceFrequency(&clock_freq);
		have_clock_freq = true;
	}

	return clock_freq.QuadPart;
}

uint64_t gettime_in_ns()
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clock_freq();

	return (uint64_t)time_val;
}

uint64_t mul_div64(uint64_t num, uint64_t mul, uint64_t div)
{
	const uint64_t rem = num % div;
	return (num / div) * mul + (rem * mul) / div;
}

static bool GetWASDeviceInfo(IMMDevice *device, WASDeviceInfo* info)
{
	IPropertyStore* store = NULL;
	WCHAR* w_id = NULL;
	HRESULT hr;

	hr = device->GetId(&w_id);
	if (FAILED(hr) || !w_id)
	{
		return false;
	}
	if (!*w_id)
	{
		CoTaskMemFree(w_id);
		return false;
	}

	char deviceIDBuffer[512] = { 0 };
	int len = wchar_to_utf8(w_id, 0, deviceIDBuffer, 511);
	if (len)
	{
		info->id = std::string(deviceIDBuffer, len);
	}
	CoTaskMemFree(w_id);

	if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)))
	{
		PROPVARIANT var;
		PropVariantInit(&var);

		hr = store->GetValue(MY_PKEY_Device_FriendlyName, &var);
		if (SUCCEEDED(hr))
		{
			char deviceNameBuffer[512] = { 0 };
			int len = wchar_to_utf8(var.pwszVal, 0, deviceNameBuffer, 511);
			if (len)
			{
				info->name = std::string(deviceNameBuffer, len);
			}
		}

		PropVariantClear(&var);
		hr = store->GetValue(MY_PKEY_AudioEngine_DeviceFormat, &var);
		if (SUCCEEDED(hr))
		{
			memcpy(&info->format, var.blob.pBlobData, min(var.blob.cbSize, sizeof(WAVEFORMATEXTENSIBLE)));
			PropVariantClear(&var);
		}
	}

	store->Release();

	return true;
}

std::string GetWASDeviceName(IMMDevice *device)
{
	std::string deviceName;
	IPropertyStore* store = NULL;
	HRESULT hr;

	if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) 
	{
		PROPVARIANT nameVar;

		PropVariantInit(&nameVar);
		hr = store->GetValue(MY_PKEY_Device_FriendlyName, &nameVar);

		if (SUCCEEDED(hr) && nameVar.pwszVal && *nameVar.pwszVal)
		{
			char deviceNameBuffer[512] = { 0 };
			int len = wchar_to_utf8(nameVar.pwszVal, 0, deviceNameBuffer, 511);

			if (len)
			{
				deviceName = std::string(deviceNameBuffer, len);
			}
			/*
			int len = WideCharToMultiByte(CP_UTF8, 0, nameVar.pwszVal, -1, 0, 0, 0, 0);
			char *s = new (std::nothrow)char[len];
			if (s)
			{
				WideCharToMultiByte(CP_UTF8, 0, nameVar.pwszVal, -1, s, len, 0, 0);
				deviceName = std::string(s, len);
				delete[] s;
			}*/
		}
		PropVariantClear(&nameVar);
		store->Release();
	}

	return deviceName;
}

bool EnumAllWASDevices(std::vector<WASDeviceInfo> &devices, bool capture)
{
	devices.clear();

	bool result = false;
	IMMDeviceEnumerator* enumerator = NULL;
	IMMDeviceCollection* collection = NULL;
	UINT count;
	HRESULT hr;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void **)&enumerator);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = enumerator->EnumAudioEndpoints(capture ? eCapture : eRender,  
		DEVICE_STATE_ACTIVE, &collection);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	hr = collection->GetCount(&count);
	if (FAILED(hr))
	{
		goto errorFlag;
	}

	for (UINT i = 0; i < count; i++)
	{
		IMMDevice* device = NULL;
		WASDeviceInfo info;

		hr = collection->Item(i, &device);
		if (FAILED(hr))
		{
			continue;
		}

		if (GetWASDeviceInfo(device, &info))
		{
			devices.push_back(info);
		}

		device->Release();
	}

	result = true;
errorFlag:
	if (enumerator)
	{
		enumerator->Release();
	}

	if (collection)
	{
		collection->Release();
	}

	return result;
}

bool init_wasapi_com()
{
	if (!IsWindowsVistaOrGreater())
	{
		return false;
	}

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (hr == S_FALSE)
	{
		hr = S_OK;
	}

	g_libAVRT = LoadLibraryA("avrt.dll");
	if (g_libAVRT)
	{
		MY_AvSetMmThreadCharacteristicsA = (pf_AvSetMmThreadCharacteristicsA)GetProcAddress(g_libAVRT, "AvSetMmThreadCharacteristicsW");
		MY_AvRevertMmThreadCharacteristics = (pf_AvRevertMmThreadCharacteristics)GetProcAddress(g_libAVRT, "AvRevertMmThreadCharacteristics");
	}
	else
	{
		MY_AvSetMmThreadCharacteristicsA = NULL;
		MY_AvRevertMmThreadCharacteristics = NULL;
	}

	return SUCCEEDED(hr);
}

void deinit_wasapi_com()
{
	MY_AvSetMmThreadCharacteristicsA = NULL;
	MY_AvRevertMmThreadCharacteristics = NULL;

	if (g_libAVRT) 
	{
		FreeLibrary(g_libAVRT);
		g_libAVRT = NULL;
	}

	CoUninitialize();
}

WASAPIFormat waveformat_to_wasapiformat(WAVEFORMATEX *waveformat)
{
	if ((waveformat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) && (waveformat->wBitsPerSample == 32))
	{
		return WASAPIFormat::WASAPI_F32SYS;
	}
	else if ((waveformat->wFormatTag == WAVE_FORMAT_PCM) && (waveformat->wBitsPerSample == 16))
	{
		return WASAPIFormat::WASAPI_S16SYS;
	}
	else if ((waveformat->wFormatTag == WAVE_FORMAT_PCM) && (waveformat->wBitsPerSample == 32))
	{
		return WASAPIFormat::WASAPI_S32SYS;
	}
	else if (waveformat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)waveformat;
		if ((memcmp(&ext->SubFormat, &MY_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) && (waveformat->wBitsPerSample == 32))
		{
			return WASAPIFormat::WASAPI_F32SYS;
		}
		else if ((memcmp(&ext->SubFormat, &MY_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (waveformat->wBitsPerSample == 16))
		{
			return WASAPIFormat::WASAPI_S16SYS;
		}
		else if ((memcmp(&ext->SubFormat, &MY_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (waveformat->wBitsPerSample == 32))
		{
			return WASAPIFormat::WASAPI_S32SYS;
		}
	}

	return WASAPIFormat::Unknown;
}

bool reset_format_to_16bits(WAVEFORMATEX *pwfx)
{
	bool ret = false;

	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
		pwfx->wBitsPerSample = 16;
		pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
		pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;

		ret = true;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		PWAVEFORMATEXTENSIBLE pEx = (PWAVEFORMATEXTENSIBLE)(pwfx);
		if (memcmp(&pEx->SubFormat, &MY_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0)
		{
			pEx->SubFormat = MY_KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
			pwfx->wBitsPerSample = 16;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;

			ret = true;
		}
	}

	return ret;
}

bool reset_format_from(WAVEFORMATEX *pwfx, uint32_t sampleRate, uint32_t channels, WASAPIFormat format)
{
	if (format == WASAPIFormat::WASAPI_F32SYS)
	{
		if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			PWAVEFORMATEXTENSIBLE pEx = (PWAVEFORMATEXTENSIBLE)(pwfx);
			pEx->SubFormat = MY_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
			pEx->Samples.wValidBitsPerSample = 32;
		}
		else
		{
			pwfx->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		}
		pwfx->wBitsPerSample = 32;
	}
	else if (format == WASAPIFormat::WASAPI_S16SYS)
	{
		if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			PWAVEFORMATEXTENSIBLE pEx = (PWAVEFORMATEXTENSIBLE)(pwfx);
			pEx->SubFormat = MY_KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
		}
		else
		{
			pwfx->wFormatTag = WAVE_FORMAT_PCM;
		}
		pwfx->wBitsPerSample = 16;
	}
	else if (format == WASAPIFormat::WASAPI_S32SYS)
	{
		if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			PWAVEFORMATEXTENSIBLE pEx = (PWAVEFORMATEXTENSIBLE)(pwfx);
			pEx->SubFormat = MY_KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 32;
		}
		else
		{
			pwfx->wFormatTag = WAVE_FORMAT_PCM;
		}
		pwfx->wBitsPerSample = 32;
	}
	else
	{
		return false;
	}

	pwfx->nSamplesPerSec = sampleRate;
	pwfx->nChannels = channels;

	pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;

	return true;
}

std::string wasapiformat_to_string(WASAPIFormat fmt)
{
	switch (fmt)
	{
	case WASAPIFormat::WASAPI_F32SYS:
		return "WASAPI_F32SYS";
	case WASAPIFormat::WASAPI_S16SYS:
		return "WASAPI_S16SYS";
	case WASAPIFormat::WASAPI_S32SYS:
		return "WASAPI_S32SYS";
	case WASAPIFormat::Unknown:
	default:
		return "Unknown";
	}
}