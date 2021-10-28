#ifndef _H_WASAPI_UTILS_H_
#define _H_WASAPI_UTILS_H_

#include <string>
#include <vector>
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <wtypes.h>

typedef HANDLE(WINAPI *pf_AvSetMmThreadCharacteristicsA)(LPCSTR TaskName, LPDWORD TaskIndex);
typedef BOOL(WINAPI * pf_AvRevertMmThreadCharacteristics)(HANDLE AvrtHandle);

extern pf_AvSetMmThreadCharacteristicsA MY_AvSetMmThreadCharacteristicsA;
extern pf_AvRevertMmThreadCharacteristics MY_AvRevertMmThreadCharacteristics;

const GUID MY_KSDATAFORMAT_SUBTYPE_PCM = { 0x00000001, 0x0000, 0x0010,{ 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID MY_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010,{ 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

enum class WASAPIFormat
{
	Unknown,

	//float 32 bit
	WASAPI_F32SYS,
	//signed 16 bit
	WASAPI_S16SYS,
	//signed 32 bit
	WASAPI_S32SYS
};

std::string wasapiformat_to_string(WASAPIFormat fmt);

//windows audio session device info
struct WASDeviceInfo
{
	std::string name;
	std::string id;
	WAVEFORMATEXTENSIBLE format;
};

std::string GetWASDeviceName(IMMDevice *device);
bool EnumAllWASDevices(std::vector<WASDeviceInfo> &devices, bool capture);

uint64_t gettime_in_ns();
uint64_t mul_div64(uint64_t num, uint64_t mul, uint64_t div);

bool init_wasapi_com();
void deinit_wasapi_com();

WASAPIFormat waveformat_to_wasapiformat(WAVEFORMATEX *waveformat);
bool reset_format_to_16bits(WAVEFORMATEX *pwfx);
bool reset_format_from(WAVEFORMATEX *pwfx, uint32_t sampleRate, uint32_t channels, WASAPIFormat format);
#endif