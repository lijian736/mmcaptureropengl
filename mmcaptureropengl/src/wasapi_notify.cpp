#include "wasapi_notify.h"
#include "wasapi_device.h"

WASAPINotify::WASAPINotify(WASAPIDevice* device)
	:m_ref_count(1), m_device(device)
{
}

WASAPINotify::~WASAPINotify()
{}

STDMETHODIMP WASAPINotify::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown)
	{
		AddRef();
		*ppv = (IUnknown *)this;
	}
	else if (riid == __uuidof(IMMNotificationClient))
	{
		AddRef();
		*ppv = (IMMNotificationClient *)this;
	}
	else
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	return S_OK;
}

STDMETHODIMP_(ULONG) WASAPINotify::AddRef()
{
	return InterlockedIncrement(&m_ref_count);
}

STDMETHODIMP_(ULONG) WASAPINotify::Release()
{
	if (!InterlockedDecrement(&m_ref_count))
	{
		delete this;
		return 0;
	}

	return m_ref_count;
}

HRESULT WASAPINotify::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
	m_device->set_default_device(flow, role, pwstrDefaultDeviceId);
	return S_OK;
}

HRESULT WASAPINotify::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT WASAPINotify::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT WASAPINotify::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	return S_OK;
}

HRESULT WASAPINotify::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
	return S_OK;
}
