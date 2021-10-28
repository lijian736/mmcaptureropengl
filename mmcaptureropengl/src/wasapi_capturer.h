#ifndef _H_WASAPI_CAPTURER_H_
#define _H_WASAPI_CAPTURER_H_

#include <map>
#include <string>
#include <functional>
#include <stdint.h>
#include "wasapi_device.h"

struct WASAPISample
{
	uint8_t* buffer;
	WASAPIFormat format; //the data format
	uint32_t channels;  //channel number
	uint32_t frames;  //number of frames
	uint32_t sampleRate; //the sample rate
	uint64_t timestamp;  //nanosecond
};

typedef std::function<void(struct WASAPISample*)> WASAPISampleCallback;

//the audio capturer. It can capture input devices and render devices
class WASAPICapturer: public WASAPIDevice
{
public:
	/**
	* @brief constructor
	* @params inputDevice -- whether the device is input device?
	*/
	WASAPICapturer( bool inputDevice);
	virtual ~WASAPICapturer();

	/**
	* @brief initialize
	*/
	bool initialize();

	/**
	* @brief set the callback
	* @params sampleCallback -- the sample callback
	*/
	void set_sample_callback(WASAPISampleCallback sampleCallback);

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
	* @brief start to capture the audio
	*/
	void start_capture();

	/**
	* @brief stop capturing the audio
	*/
	void stop_capture();

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
	* @brief initialize the capture
	*/
	bool initialize_capture();

	/**
	* @brief free the resources
	*/
	void free_resources();

	/**
	* @brief process the captured data
	*/
	bool process_capture_data();

	/**
	* @brief reconnect to the device
	*/
	bool reconnect();

	/**
	* @brief reconnect thread function
	*/
	static DWORD WINAPI reconnect_thread(LPVOID param);

	/**
	* @brief capture thread function
	*/
	static DWORD WINAPI capture_thread(LPVOID param);

private:

	//the device enumerator
	IMMDeviceEnumerator* m_enumerator;
	//the multimedia device
	IMMDevice* m_mmdevice;
	//the audio client
	IAudioClient* m_client;
	//the audio capture client
	IAudioCaptureClient* m_capture;
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
	//is the device an input device?
	bool m_is_input_device;
	//use the device timing?
	bool m_use_device_timing;
	//is the device default?
	bool m_is_default_device;
	//the device is capturing?
	bool m_is_capturing;
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

	//the stop capturing event
	HANDLE m_stop_event;

	//the capture thread
	HANDLE m_capture_thread_handle;
	//the reconnect thread handle
	HANDLE m_reconnect_thread_handle;

	//the sample callback
	WASAPISampleCallback m_sample_callback;
	struct WASAPISample m_sample;

	//the client buffer frames number
	UINT32 m_num_buffer_frames;
};


#endif