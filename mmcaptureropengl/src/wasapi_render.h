#ifndef _H_WASAPI_RENDER_H_
#define _H_WASAPI_RENDER_H_

#include <map>
#include <string>
#include <stdint.h>

#include "wasapi_device.h"

//the audio render.
class WASAPIRender : public WASAPIDevice
{
public:
	/**
	* @brief constructor
	* @params settings -- the config settings
	*/
	WASAPIRender(std::map<std::string, std::string>& settings);
	virtual ~WASAPIRender();

	/**
	* @brief initialize
	*/
	bool initialize();

	/**
	* @brief update the config settings
	* @params settings -- the new config settings
	*/
	void update(std::map<std::string, std::string>& settings);

	/**
	* @brief if the default device was changed, this function will be called.
	*/
	virtual void set_default_device(EDataFlow flow, ERole role, LPCWSTR id) override;

	/**
	* @brief start to render
	*/
	void start_render();

	/**
	* @brief stop the render
	*/
	void stop_render();

	/**
	* @brief render the data
	* @params buffer -- the data to reder
	*         len -- the data length
	*/
	bool render_data(uint8_t* buffer, int len);

protected:

	/**
	* @brief update the config settings
	* @params settings -- the config settings
	*/
	void update_settings(std::map<std::string, std::string>& settings);

	/**
	* @brief try to initialize the object
	*/
	bool try_initialize();

	/**
	* @brief initialize the object
	*/
	bool inner_initialize();

	/**
	* @brief initialize the device
	*/
	bool initialize_device();

	/**
	* @brief initialize the client
	*/
	bool initialize_client();

	/**
	* @brief initialize the render
	*/
	bool initialize_render();

	/**
	* @brief free the resources
	*/
	void free_resources();

	/**
	* @brief reconnect to the device
	*/
	bool reconnect();

	/**
	* @brief reconnect thread function
	*/
	static DWORD WINAPI reconnect_thread(LPVOID param);

private:

	//the device enumerator
	IMMDeviceEnumerator* m_enumerator;
	//the multimedia device
	IMMDevice* m_mmdevice;
	//the audio client
	IAudioClient* m_client;
	//the audio render client
	IAudioRenderClient* m_render;
	//the notification client
	IMMNotificationClient* m_notify;

	//default id
	std::wstring m_default_id;
	//device id
	std::string m_device_id;
	//device name
	std::string m_device_name;

	//is the device default?
	bool m_is_default_device;
	//is the device playing?
	bool m_is_playing;
	//is the device reconnecting?
	bool m_is_reconnecting;

	//sample rate
	uint32_t m_sample_rate;
	//the channels
	uint32_t m_sample_channels;
	//the format
	WASAPIFormat m_wasapi_format;
	//the last notify time
	uint64_t m_last_notify_time;
	uint32_t m_block_align;

	//the buffer frames count
	UINT32 m_buffer_frame_count;

	//the stop event
	HANDLE m_stop_event;
	//the reconnect thread handle
	HANDLE m_reconnect_thread_handle;
};

#endif
