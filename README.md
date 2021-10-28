## Windows平台音视频流捕获软件
本软件捕获windows系统上的视频流和音频流，并进行渲染。

捕获视频流，使用的是微软的DirectShow组件。
捕获音频流，使用的是微软的DirectShow组件，以及WASAPI。

视频流渲染，使用的是OpenGL着色器进行渲染。
音频流的播放，使用的是WASAPI。

解码使用的是FFmpeg库，64位版本。

IDE使用的Visual Studio 2015社区版。该工程代码下载后直接编译即可运行。  
由于FFmpeg库太大，使用者需要自行下载。  
该系统使用的具体的FFmpeg版本为：ffmpeg-4.4-full_build-shared。    
UI使用MFC进行绘制。

第三方库的目录为：  
mmcaptureropengl/library/ffmpeg-4.4-full_build-shared  

软件截图:
![Image text](https://gitee.com/videoaudioer/mmcapturer/raw/master/screenshot/screenshot.png)