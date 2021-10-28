
// mmcaptureropenglDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "mmcaptureropengl.h"
#include "mmcaptureropenglDlg.h"
#include "afxdialogex.h"

#include <new>
#include <comdef.h>
#include <dshow.h>

static const struct VIDEO_FORMAT_ENTRY {
	enum AVPixelFormat ffmpegFormat;
	VideoFormat videoFormat;

} VIDEO_FORMAT_MAP[] = {
	{ AV_PIX_FMT_YUV420P, VideoFormat::IYUV },
	{ AV_PIX_FMT_YUYV422, VideoFormat::YUY2 },
	{ AV_PIX_FMT_NV12, VideoFormat::NV12 },
	{ AV_PIX_FMT_UYVY422, VideoFormat::UYVY },
	{ AV_PIX_FMT_ARGB, VideoFormat::ARGB32 },
	{ AV_PIX_FMT_YVYU422, VideoFormat::YVYU },
	{ AV_PIX_FMT_RGB555,  VideoFormat::RGB555 },
	{ AV_PIX_FMT_RGB565,  VideoFormat::RGB565 },
	{ AV_PIX_FMT_RGB24,   VideoFormat::RGB24 },
};


// CmmcaptureropenglDlg 对话框



CmmcaptureropenglDlg::CmmcaptureropenglDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_MMCAPTUREROPENGL_DIALOG, pParent),
	m_radio_play_type(0), m_render_video_thread_handle(NULL),
	m_render_video_event_handle(NULL), m_stop_video_event_handle(NULL),
	m_render_audio_thread_handle(NULL), m_render_audio_event_handle(NULL),
	m_stop_audio_event_handle(NULL), m_has_error(FALSE),
	m_media_box(NULL), m_ffmpeg_decoder(NULL),
	m_ffmpeg_transcoder(NULL), m_wasapi_capture(NULL), m_wasapi_render(NULL),
	m_radio_audio_type(0), m_radio_wasapi_type(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CmmcaptureropenglDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_VIDEOS, m_combo_videos);
	DDX_Control(pDX, IDC_COMBO_AUDIOS, m_combo_audios);
	DDX_Radio(pDX, IDC_RADIO_VIDEO_AUDIO, m_radio_play_type);
	DDX_Radio(pDX, IDC_RADIO_DSHOW, m_radio_audio_type);
	DDX_Radio(pDX, IDC_RADIO_MICROPHONE, m_radio_wasapi_type);
}

BEGIN_MESSAGE_MAP(CmmcaptureropenglDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_QUERY, &CmmcaptureropenglDlg::OnBnClickedBtnQuery)
	ON_BN_CLICKED(IDC_BTN_PLAY, &CmmcaptureropenglDlg::OnBnClickedBtnPlay)
	ON_BN_CLICKED(IDC_BTN_EXIT, &CmmcaptureropenglDlg::OnBnClickedBtnExit)
	ON_BN_CLICKED(IDC_RADIO_VIDEO_AUDIO, &CmmcaptureropenglDlg::OnBnClickedRadioPlayType)
	ON_BN_CLICKED(IDC_RADIO_VIDEO, &CmmcaptureropenglDlg::OnBnClickedRadioPlayType)
	ON_BN_CLICKED(IDC_RADIO_AUDIO, &CmmcaptureropenglDlg::OnBnClickedRadioPlayType)
	ON_BN_CLICKED(IDC_RADIO_DSHOW, &CmmcaptureropenglDlg::OnBnClickedRadioDshow)
	ON_BN_CLICKED(IDC_RADIO_WASAPI, &CmmcaptureropenglDlg::OnBnClickedRadioDshow)
	ON_BN_CLICKED(IDC_RADIO_MICROPHONE, &CmmcaptureropenglDlg::OnBnClickedRadioWASAPI)
	ON_BN_CLICKED(IDC_RADIO_SPEAKER, &CmmcaptureropenglDlg::OnBnClickedRadioWASAPI)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CmmcaptureropenglDlg 消息处理程序

BOOL CmmcaptureropenglDlg::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此添加专用代码和/或调用基类
	cs.style |= (WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	return CDialogEx::PreCreateWindow(cs);
}

BOOL CmmcaptureropenglDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	m_render_video_thread_handle = NULL;
	m_render_video_event_handle = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!m_render_video_event_handle)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_stop_video_event_handle = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!m_stop_video_event_handle)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_render_audio_thread_handle = NULL;
	m_render_audio_event_handle = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!m_render_audio_event_handle)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_stop_audio_event_handle = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!m_stop_audio_event_handle)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_media_box = new (std::nothrow) MediaBox();
	if (!m_media_box)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}
	else
	{
		m_media_box->set_video_sample_callback(std::bind(&CmmcaptureropenglDlg::on_video_sample, this));
		m_media_box->set_audio_sample_callback(std::bind(&CmmcaptureropenglDlg::on_audio_sample, this));
	}

	if (!init_wasapi_com())
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_ffmpeg_decoder = new (std::nothrow) FFmpegDecoder();
	if (!m_ffmpeg_decoder)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	m_ffmpeg_transcoder = new (std::nothrow) FFmpegTranscoder();
	if (!m_ffmpeg_transcoder)
	{
		m_has_error = TRUE;
		goto exitFlag;
	}

	memset(&m_opengl_ctx, 0, sizeof(m_opengl_ctx));
	m_opengl_ctx.hdc = GetDlgItem(IDC_STATIC_PREVIEW)->GetDC()->GetSafeHdc();
	m_opengl_ctx.hwnd = GetDlgItem(IDC_STATIC_PREVIEW)->GetSafeHwnd();

	if (!create_window_opengl(&m_opengl_ctx))
	{
		LOG_DEBUG("init_window_opengl() failed");
		m_has_error = TRUE;
		goto exitFlag;
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE

exitFlag:
	if (m_render_video_event_handle)
	{
		CloseHandle(m_render_video_event_handle);
		m_render_video_event_handle = NULL;
	}

	if (m_stop_video_event_handle)
	{
		CloseHandle(m_stop_video_event_handle);
		m_stop_video_event_handle = NULL;
	}

	if (m_render_audio_event_handle)
	{
		CloseHandle(m_render_audio_event_handle);
		m_render_audio_event_handle = NULL;
	}

	if (m_stop_audio_event_handle)
	{
		CloseHandle(m_stop_audio_event_handle);
		m_stop_audio_event_handle = NULL;
	}

	if (m_media_box)
	{
		delete m_media_box;
		m_media_box = NULL;
	}

	if (m_ffmpeg_decoder)
	{
		delete m_ffmpeg_decoder;
		m_ffmpeg_decoder = NULL;
	}

	if (m_ffmpeg_transcoder)
	{
		delete m_ffmpeg_transcoder;
		m_ffmpeg_transcoder = NULL;
	}

	if (m_wasapi_capture)
	{
		delete m_wasapi_capture;
		m_wasapi_capture = NULL;
	}

	return TRUE;
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CmmcaptureropenglDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CmmcaptureropenglDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CmmcaptureropenglDlg::flush_preview_window()
{
	flush_window_opengl(&m_opengl_ctx);
}

DWORD WINAPI CmmcaptureropenglDlg::render_video_thread(LPVOID ptr)
{
	LOG_INFO("Render video thread starts........");

	CmmcaptureropenglDlg* dlg = (CmmcaptureropenglDlg*)ptr;
	dlg->render_video_run();
	dlg->flush_preview_window();

	LOG_INFO("Render video thread ends........");

	return 0;
}

void CmmcaptureropenglDlg::render_video_run()
{
	HANDLE signals[2] = { m_render_video_event_handle, m_stop_video_event_handle };
	DWORD ret = WaitForMultipleObjects(2, signals, FALSE, INFINITE);
	while (ret == WAIT_OBJECT_0)
	{
		while (VideoRawData* raw = m_media_box->begin_raw_video_data())
		{
			if (raw->format == VideoFormat::H264)
			{
				decode_video(AV_CODEC_ID_H264, raw->data, raw->used, raw->startTime, raw->stopTime);
			}
			else if (raw->format == VideoFormat::MJPEG)
			{
				decode_video(AV_CODEC_ID_MJPEG, raw->data, raw->used, raw->startTime, raw->stopTime);
			}
			else if (raw->format == VideoFormat::Any || raw->format == VideoFormat::Unknown)
			{
				LOG_ERROR("Unknown data arrived.");
			}
			else
			{
				process_yuv_or_rgb(raw->data, raw->used, raw->startTime, raw->stopTime, raw->format,
					raw->width, raw->height);
			}

			m_media_box->end_raw_video_data(raw);
		}

		ret = WaitForMultipleObjects(2, signals, FALSE, INFINITE);
	}
}

bool CmmcaptureropenglDlg::decode_video(const AVCodecID& codecID, unsigned char *data,
	size_t size, REFERENCE_TIME startTime, REFERENCE_TIME stopTime)
{
	if (!m_ffmpeg_decoder->is_initialized() || !m_ffmpeg_decoder->validate(codecID))
	{
		bool ret = m_ffmpeg_decoder->init(codecID);
		if (!ret)
		{
			return false;
		}
	}

	AVFrame* frame;
	VideoFrameRender render;
	if (m_ffmpeg_decoder->send_video_data(data, size, startTime))
	{
		while ((frame = m_ffmpeg_decoder->receive_frame()) != NULL)
		{
			bool ret = render.InitFromFFmpeg(frame);
			if (!ret)
			{
				return false;
			}

			render.width = frame->width;
			render.height = frame->height;
			render.timestamp = frame->pts * 100;
			render.ffmpegFormat = frame->format;

			render_video_frame(render);
		}
	}

	return true;
}

bool CmmcaptureropenglDlg::process_yuv_or_rgb(unsigned char *data,
	size_t size, REFERENCE_TIME startTime, REFERENCE_TIME stopTime,
	VideoFormat format, int width, int height)
{
	VideoFrameRender render;
	AVFrame* frame;
	bool ret;

	AVPixelFormat pixelFmt = AV_PIX_FMT_NONE;
	for (int i = 0; i < sizeof(VIDEO_FORMAT_MAP) / sizeof(VIDEO_FORMAT_ENTRY); i++)
	{
		if (format == VIDEO_FORMAT_MAP[i].videoFormat)
		{
			pixelFmt = VIDEO_FORMAT_MAP[i].ffmpegFormat;
			break;
		}
	}

	if (AV_PIX_FMT_NONE == pixelFmt)
	{
		LOG_ERROR("Unsupported pixel format");
		return false;
	}

	ret = render.InitFrom(data, size, format, width, height);
	if (ret)
	{
		if (AV_PIX_FMT_YUV420P != pixelFmt)
		{
			ret = m_ffmpeg_transcoder->scale_yuv((uint8_t*)render.data, render.linesize, width, height, pixelFmt, &frame);
			bool ret = render.InitFromFFmpeg(frame);
			if (!ret)
			{
				return false;
			}
		}
	}
	if (!ret)
	{
		return false;
	}

	render.width = width;
	render.height = height;
	render.timestamp = startTime * 100;
	render.ffmpegFormat = AV_PIX_FMT_YUV420P;

	render_video_frame(render);
	return true;
}

bool CmmcaptureropenglDlg::render_video_frame(VideoFrameRender& render)
{
	render_yuv420p_by_opengl(&m_opengl_ctx, render.width, render.height,
		render.data[0], render.linesize[0], render.data[1], render.linesize[1], render.data[2], render.linesize[2]);

	return true;
}

DWORD WINAPI CmmcaptureropenglDlg::render_audio_thread(LPVOID ptr)
{
	LOG_INFO("Render audio thread starts........");

	CmmcaptureropenglDlg* dlg = (CmmcaptureropenglDlg*)ptr;
	dlg->render_audio_run();

	LOG_INFO("Render audio thread ends........");

	return 0;
}

void CmmcaptureropenglDlg::render_audio_run()
{
	HANDLE signals[2] = { m_render_audio_event_handle, m_stop_audio_event_handle };
	DWORD ret = WaitForMultipleObjects(2, signals, FALSE, INFINITE);
	while (ret == WAIT_OBJECT_0)
	{
		while (AudioRawData* raw = m_media_box->begin_raw_audio_data())
		{
			if (!m_wasapi_render)
			{
				std::map<std::string, std::string> configs;
				configs["device_id"] = "default";
				configs["sample_rate"] = std::to_string(raw->sampleRate);
				configs["channels"] = std::to_string(raw->channels);
				if (raw->format == AudioFormat::Wave16bit)
				{
					configs["wasapi_format"] = "WASAPI_S16SYS";
				}
				else if (raw->format == AudioFormat::WaveFloat)
				{
					configs["wasapi_format"] = "WASAPI_F32SYS";
				}
				else
				{
					LOG_ERROR("The audio format is not supported.");
					m_media_box->end_raw_audio_data(raw);
					continue;
				}

				m_wasapi_render = new (std::nothrow) WASAPIRender(configs);
				if (!m_wasapi_render)
				{
					LOG_DEBUG("Create WASAPIRender failed.");
					m_media_box->end_raw_audio_data(raw);
					continue;
				}
				else
				{
					if (!m_wasapi_render->initialize())
					{
						delete m_wasapi_render;
						m_wasapi_render = NULL;
						m_media_box->end_raw_audio_data(raw);

						LOG_DEBUG("WASAPIRender initialize failed.");
						continue;
					}
					m_wasapi_render->start_render();
				}
			}
			
			m_wasapi_render->render_data(raw->data, raw->used);
			m_media_box->end_raw_audio_data(raw);
		}
		ret = WaitForMultipleObjects(2, signals, FALSE, INFINITE);
	}

}

void CmmcaptureropenglDlg::on_video_sample()
{
	SetEvent(m_render_video_event_handle);
}

void CmmcaptureropenglDlg::on_audio_sample()
{
	SetEvent(m_render_audio_event_handle);
}

void CmmcaptureropenglDlg::on_wasapi_sample(struct WASAPISample* sample)
{
	int len = 0;
	int bytesNum = 0;

	if (!sample->buffer)
	{
		return;
	}

	if (sample->format == WASAPIFormat::WASAPI_F32SYS || sample->format == WASAPIFormat::WASAPI_S32SYS)
	{
		bytesNum = 4;
	}
	else if (sample->format == WASAPIFormat::WASAPI_S16SYS)
	{
		bytesNum = 2;
	}

	if (!m_wasapi_render)
	{
		std::map<std::string, std::string> configs;
		configs["device_id"] = "default";
		configs["sample_rate"] = std::to_string(sample->sampleRate);
		configs["channels"] = std::to_string(sample->channels);
		configs["wasapi_format"] = wasapiformat_to_string(sample->format);

		m_wasapi_render = new (std::nothrow) WASAPIRender(configs);
		if (!m_wasapi_render)
		{
			LOG_DEBUG("Create WASAPIRender failed.");
			return;
		}
		else
		{
			if (!m_wasapi_render->initialize())
			{
				delete m_wasapi_render;
				m_wasapi_render = NULL;
				return;
			}
			m_wasapi_render->start_render();
		}
	}
	m_wasapi_render->render_data(sample->buffer, sample->channels * bytesNum * sample->frames);
}

void CmmcaptureropenglDlg::play_video_audio()
{
	int vsel = m_combo_videos.GetCurSel();
	if (vsel < 0)
	{
		return;
	}

	int asel = m_combo_audios.GetCurSel();
	if (asel < 0)
	{
		return;
	}

	m_render_video_thread_handle = CreateThread(NULL, NULL, CmmcaptureropenglDlg::render_video_thread, this, NULL, NULL);
	if (!m_render_video_thread_handle)
	{
		LOG_ERROR("Start render video thread error.");
		return;
	}

	if (m_radio_audio_type == 0)
	{
		m_render_audio_thread_handle = CreateThread(NULL, NULL, CmmcaptureropenglDlg::render_audio_thread, this, NULL, NULL);
		if (!m_render_audio_thread_handle)
		{
			LOG_ERROR("Start render audio thread error.");
			return;
		}
	}
	else
	{
		if (m_radio_wasapi_type == 0)
		{
			m_wasapi_capture = new (std::nothrow) WASAPICapturer(true);
		}
		else
		{
			m_wasapi_capture = new (std::nothrow) WASAPICapturer(false);
		}
		if (!m_wasapi_capture || !m_wasapi_capture->initialize())
		{
			return;
		}
		else
		{
			m_wasapi_capture->set_sample_callback(std::bind(&CmmcaptureropenglDlg::on_wasapi_sample, this, std::placeholders::_1));
			std::map<std::string, std::string> configs;
			configs["device_id"] = m_wasapi_devices[asel].id;
			configs["use_device_timing"] = "true";

			m_wasapi_capture->update(configs);
			m_wasapi_capture->start_capture();
		}
	}

	std::map<std::string, std::wstring> videoConfigs;
	videoConfigs["name"] = m_videos_vec[vsel].namew;
	videoConfigs["path"] = m_videos_vec[vsel].pathw;
	m_media_box->set_video_config(videoConfigs);

	if (m_radio_audio_type == 0)
	{
		std::map<std::string, std::wstring> audioConfigs;
		audioConfigs["name"] = m_audios_vec[asel].namew;
		audioConfigs["path"] = m_audios_vec[asel].pathw;

		m_media_box->set_audio_config(audioConfigs);
		m_media_box->queue_command(Command::ActivateAll);
	}
	else
	{
		m_media_box->queue_command(Command::ActivateVideo);
	}
}

void CmmcaptureropenglDlg::play_video()
{
	int vsel = m_combo_videos.GetCurSel();
	if (vsel < 0)
	{
		return;
	}

	m_render_video_thread_handle = CreateThread(NULL, NULL, CmmcaptureropenglDlg::render_video_thread, this, NULL, NULL);
	if (!m_render_video_thread_handle)
	{
		LOG_ERROR("Start render video thread error.");
		return;
	}

	std::map<std::string, std::wstring> videoConfigs;
	videoConfigs["name"] = m_videos_vec[vsel].namew;
	videoConfigs["path"] = m_videos_vec[vsel].pathw;

	m_media_box->set_video_config(videoConfigs);
	m_media_box->queue_command(Command::ActivateVideo);
}

void CmmcaptureropenglDlg::play_audio()
{
	int asel = m_combo_audios.GetCurSel();
	if (asel < 0)
	{
		return;
	}

	if (m_radio_audio_type == 0)
	{
		m_render_audio_thread_handle = CreateThread(NULL, NULL, CmmcaptureropenglDlg::render_audio_thread, this, NULL, NULL);
		if (!m_render_audio_thread_handle)
		{
			LOG_ERROR("Start render audio thread error.");
			return;
		}

		std::map<std::string, std::wstring> audioConfigs;
		audioConfigs["name"] = m_audios_vec[asel].namew;
		audioConfigs["path"] = m_audios_vec[asel].pathw;

		m_media_box->set_audio_config(audioConfigs);
		m_media_box->queue_command(Command::ActivateAudio);
	}
	else
	{
		if (m_radio_wasapi_type == 0)
		{
			m_wasapi_capture = new (std::nothrow) WASAPICapturer(true);
		}
		else
		{
			m_wasapi_capture = new (std::nothrow) WASAPICapturer(false);
		}
		if (!m_wasapi_capture || !m_wasapi_capture->initialize())
		{
			return;
		}
		else
		{
			m_wasapi_capture->set_sample_callback(std::bind(&CmmcaptureropenglDlg::on_wasapi_sample, this, std::placeholders::_1));
			std::map<std::string, std::string> configs;
			configs["device_id"] = m_wasapi_devices[asel].id;
			configs["use_device_timing"] = "true";

			m_wasapi_capture->update(configs);
			m_wasapi_capture->start_capture();
		}
	}
}

void CmmcaptureropenglDlg::OnBnClickedBtnQuery()
{
	LOG_INFO("begin to query video/audio devices.......");

	m_combo_videos.ResetContent();
	//query video devices
	HRESULT hr = EnumAllVideoDevices(m_videos_vec);
	if (FAILED(hr))
	{
		LOG_ERROR("query video devices error.");
		return;
	}

	LOG_INFO("[%d] video devices found", m_videos_vec.size());
	for (int i = 0; i < m_videos_vec.size(); i++)
	{
		m_videos_vec[i].ConvertString();

		LOG_INFO("Video Device[%d] name:[%s], path:[%s]", i, m_videos_vec[i].name.c_str(), m_videos_vec[i].path.c_str());
		m_combo_videos.AddString(_T(m_videos_vec[i].name.c_str()));
	}
	m_combo_videos.SetCurSel(0);

	//query audio devices
	m_combo_audios.ResetContent();
	if (m_radio_audio_type == 0)
	{
		hr = EnumAllAudioInputDevices(m_audios_vec);
		if (FAILED(hr))
		{
			LOG_ERROR("query audio devices error.");
			return;
		}

		LOG_INFO("[%d] audio input devices found", m_audios_vec.size());
		for (int i = 0; i < m_audios_vec.size(); i++)
		{
			m_audios_vec[i].ConvertString();
			if (is_utf8_str(m_audios_vec[i].name))
			{
				LOG_INFO("Audio Device[%d] name:[%s], path:[%s]", i, utf8_to_gbk(m_audios_vec[i].name).c_str(), m_audios_vec[i].path.c_str());
				m_combo_audios.AddString(_T(utf8_to_gbk(m_audios_vec[i].name).c_str()));
			}
			else
			{
				m_combo_audios.AddString(_T(m_audios_vec[i].name).c_str());
			}
		}
	}
	else
	{
		bool isCapture = false;
		if (m_radio_wasapi_type == 0)
		{
			isCapture = true;
		}
		bool ret = EnumAllWASDevices(m_wasapi_devices, isCapture);
		if (!ret)
		{
			LOG_ERROR("query wasapi audio devices error.");
			return;
		}

		LOG_INFO("[%d] WASAPI audio input devices found", m_wasapi_devices.size());
		for (int i = 0; i < m_wasapi_devices.size(); i++)
		{
			if (is_utf8_str(m_wasapi_devices[i].name))
			{
				LOG_INFO("WASAPI Audio Device[%d] id:[%s], name:[%s]", i, m_wasapi_devices[i].id.c_str(), utf8_to_gbk(m_wasapi_devices[i].name).c_str());
				m_combo_audios.AddString(_T(utf8_to_gbk(m_wasapi_devices[i].name).c_str()));
			}
			else
			{
				m_combo_audios.AddString(_T(m_wasapi_devices[i].name).c_str());
			}
		}
	}
	m_combo_audios.SetCurSel(0);
}


void CmmcaptureropenglDlg::OnBnClickedBtnPlay()
{
	if (m_render_video_thread_handle || m_render_audio_thread_handle || m_has_error)
	{
		return;
	}

	switch (m_radio_play_type)
	{
	case 0:  //video and audio
		play_video_audio();
		break;

	case 1:  //video
		play_video();
		break;

	case 2:  //audio
		play_audio();
		break;
	}
}


void CmmcaptureropenglDlg::OnBnClickedBtnExit()
{
	if (!(m_render_video_thread_handle || m_render_audio_thread_handle) || m_has_error)
	{
		return;
	}

	if (m_media_box)
	{
		m_media_box->queue_command(Command::DeactivateAll);
	}

	if (m_render_video_thread_handle)
	{
		SetEvent(m_stop_video_event_handle);
		WaitForSingleObject(m_render_video_thread_handle, INFINITE);
		CloseHandle(m_render_video_thread_handle);
		m_render_video_thread_handle = NULL;
	}

	if (m_render_audio_thread_handle)
	{
		SetEvent(m_stop_audio_event_handle);
		WaitForSingleObject(m_render_audio_thread_handle, INFINITE);
		CloseHandle(m_render_audio_thread_handle);
		m_render_audio_thread_handle = NULL;
	}

	if (m_wasapi_capture)
	{
		m_wasapi_capture->stop_capture();
		delete m_wasapi_capture;
		m_wasapi_capture = NULL;
	}

	if (m_wasapi_render)
	{
		m_wasapi_render->stop_render();
		delete m_wasapi_render;
		m_wasapi_render = NULL;
	}
}

void CmmcaptureropenglDlg::OnBnClickedRadioPlayType()
{
	UpdateData(TRUE);

	LOG_DEBUG("m_radio_play_type:[%d]", m_radio_play_type);
}

void CmmcaptureropenglDlg::OnBnClickedRadioDshow()
{
	UpdateData(TRUE);
	LOG_DEBUG("m_radio_audio_type:[%d]", m_radio_audio_type);

	//wasapi
	if (m_radio_audio_type == 1)
	{
		GetDlgItem(IDC_RADIO_MICROPHONE)->EnableWindow(TRUE);
		GetDlgItem(IDC_RADIO_SPEAKER)->EnableWindow(TRUE);
	}
	else
	{
		GetDlgItem(IDC_RADIO_MICROPHONE)->EnableWindow(FALSE);
		GetDlgItem(IDC_RADIO_SPEAKER)->EnableWindow(FALSE);
	}

	m_combo_audios.ResetContent();
}

void CmmcaptureropenglDlg::OnBnClickedRadioWASAPI()
{
	UpdateData(TRUE);
	LOG_DEBUG("m_radio_wasapi_type:[%d]", m_radio_wasapi_type);
}

void CmmcaptureropenglDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	OnBnClickedBtnExit();

	if (m_media_box)
	{
		delete m_media_box;
		m_media_box = NULL;
	}

	if (m_ffmpeg_decoder)
	{
		delete m_ffmpeg_decoder;
		m_ffmpeg_decoder = NULL;
	}

	if (m_ffmpeg_transcoder)
	{
		delete m_ffmpeg_transcoder;
		m_ffmpeg_transcoder = NULL;
	}

	if (m_wasapi_capture)
	{
		delete m_wasapi_capture;
		m_wasapi_capture = NULL;
	}

	if (m_wasapi_render)
	{
		delete m_wasapi_render;
		m_wasapi_render = NULL;
	}

	destroy_window_opengl(&m_opengl_ctx);
	deinit_wasapi_com();
}
