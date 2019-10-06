#include "media/devices/gtkvideorenderer.h"
#include "session/media/channelmanager.h"

#include <iostream>

using namespace cricket;
using namespace std;

class CameraVideoProcessor : public VideoProcessor
{
	GtkVideoRenderer renderer;

public :

	// Contents of frame may be manipulated by the processor.
	// The processed data is expected to be the same size as the
	// original data. VideoProcessors may be chained together and may decide
	// that the current frame should be dropped. If *dropFrame is true,
	// the current processor should skip processing. If the current processor
	// decides it cannot process the current frame in a timely manner, it may set
	// *dropFrame = true and the frame will be dropped.
	virtual void OnFrame(uint32 ssrc, VideoFrame* frame, bool* dropFrame)
	{
		if (*dropFrame) return;
		
		renderer.RenderFrame(frame);
	}
};

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s /dev/video0\n", argv[0]);
		return 0;
	}
	string camera = argv[1];

	talk_base::Thread* workerThread = new talk_base::Thread();
	workerThread->Start();

	ChannelManager cm(workerThread);
	cm.SetVideoOptions(camera);
	VideoCapturer* vc = cm.CreateVideoCapturer();
	const vector<VideoFormat>& formats = *vc->GetSupportedFormats();
	if (!formats.size())
	{
		fprintf(stderr, "Error: empty formats list for the video capturer\n");
		exit(-1);
	}
	
	CameraVideoProcessor cvp;
	vc->AddVideoProcessor(&cvp);
	
	CaptureState state = vc->Start(formats[0]);

	while (vc->IsRunning())
		continue;

	vc->Stop();
	
	return 0;
}

