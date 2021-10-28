
// mmcaptureropenglDlg.h : 头文件
//

#pragma once

#include "afxwin.h"

#include <map>
#include <string>
#include "common_lockfree_queue.h"
#include "common_logger.h"
#include "common_utf8.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_transcoder.h"
#include "sample_utils.h"
#include "sample_mediabox.h"
#include "opengl_utils.h"
#include "wasapi_capturer.h"
#include "wasapi_render.h"
#include "wasapi_utils.h"

// CmmcaptureropenglDlg 对话框
class CmmcaptureropenglDlg : public CDialogEx
{
// 构造
public:
	CmmcaptureropenglDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MMCAPTUREROPENGL_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnQuery();
	afx_msg void OnBnClickedBtnPlay();
	afx_msg void OnBnClickedBtnExit();
	afx_msg void OnBnClickedRadioPlayType();
	afx_msg void OnBnClickedRadioDshow();
	afx_msg void OnBnClickedRadioWASAPI();
	afx_msg void OnDestroy();

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	void on_video_sample();
	void on_audio_sample();
	void on_wasapi_sample(struct WASAPISample*);

private:
	void play_video_audio();
	void play_video();
	void play_audio();

	static DWORD WINAPI render_video_thread(LPVOID ptr);
	void render_video_run();
	void flush_preview_window();

	bool decode_video(const AVCodecID& codecID, unsigned char *data,
		size_t size, REFERENCE_TIME startTime, REFERENCE_TIME stopTime);

	bool process_yuv_or_rgb(unsigned char *data,
		size_t size, REFERENCE_TIME startTime, REFERENCE_TIME stopTime,
		VideoFormat format, int width, int height);

	bool render_video_frame(VideoFrameRender& render);

	void sdl_render_video(VideoFrameRender& render);
	bool init_sdl_video_context(VideoFrameRender& render);

	static DWORD WINAPI render_audio_thread(LPVOID ptr);
	void render_audio_run();

private:
	HICON m_hIcon;

	BOOL m_has_error;

	//the video render thread
	HANDLE m_render_video_thread_handle;
	//the video render event
	HANDLE m_render_video_event_handle;
	//the video stop event
	HANDLE m_stop_video_event_handle;

	//the audio render thread
	HANDLE m_render_audio_thread_handle;
	//the audio render event
	HANDLE m_render_audio_event_handle;
	//the audio stop event
	HANDLE m_stop_audio_event_handle;

	//the video combo box
	CComboBox m_combo_videos;
	//the audio combo box
	CComboBox m_combo_audios;

	//the play type
	int m_radio_play_type;
	//the audio type(DirectShow or WASAPI)
	int m_radio_audio_type;
	//the WASAPI type(microphone or speaker)
	int m_radio_wasapi_type;

	//the video list
	std::vector<VideoDeviceInfo> m_videos_vec;
	//the audio list
	std::vector<AudioDeviceInfo> m_audios_vec;
	//the wasapi audio list
	std::vector<WASDeviceInfo> m_wasapi_devices;

	//the multimedia box
	MediaBox* m_media_box;
	//the wasapi capturer
	WASAPICapturer* m_wasapi_capture;
	//the wasapi render
	WASAPIRender* m_wasapi_render;

	//FFmpeg decoder
	FFmpegDecoder* m_ffmpeg_decoder;
	//FFmpeg transcoder
	FFmpegTranscoder* m_ffmpeg_transcoder;

	struct GLWindowContext m_opengl_ctx;
};
