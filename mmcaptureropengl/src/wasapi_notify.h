#ifndef _H_WASAPI_NOTIFY_H_
#define _H_WASAPI_NOTIFY_H_

#include <windows.h>
#include <mmdeviceapi.h>

class WASAPIDevice;

class WASAPINotify : public IMMNotificationClient
{
public:
	WASAPINotify(WASAPIDevice* device);
	virtual ~WASAPINotify();

	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	/**
	* The OnDefaultDeviceChanged method notifies the client that
	* the default audio endpoint device for a particular device role has changed
	*/
	HRESULT OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);

	/**
	* The OnDeviceAdded method indicates that a new audio endpoint device has been added
	*/
	HRESULT OnDeviceAdded(LPCWSTR pwstrDeviceId);

	/**
	* The OnDeviceRemoved method indicates that an audio endpoint device has been removed
	*/
	HRESULT OnDeviceRemoved(LPCWSTR pwstrDeviceId);

	/**
	* The OnDeviceStateChanged method indicates that the state of an audio endpoint device has changed
	*/
	HRESULT OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);

	/**
	* The OnPropertyValueChanged method indicates that the value of a property
	* belonging to an audio endpoint device has changed
	*/
	HRESULT OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

private:
	volatile long m_ref_count;
	WASAPIDevice* m_device;
};

#endif