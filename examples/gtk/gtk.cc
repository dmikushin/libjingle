#include "media/devices/gtkvideorenderer.h"
#include <vector>

int main(int argc, char* argv[])
{
	cricket::GtkVideoRenderer renderer;
	
	const int width = 300;
	const int height = 200;
	
	std::vector<uint8> pixels(width * height * 4);
	
	while (1)
	{
		for (int j = 0; j < height; j++)
			for (int i = 0; i < width; i++)
			{
				pixels[4 * (j * width + i)] = rand() % 255;
				pixels[4 * (j * width + i) + 1] = rand() % 255;
				pixels[4 * (j * width + i) + 2] = rand() % 255;
				pixels[4 * (j * width + i) + 3] = rand() % 255;
			}
		
		renderer.RenderFrame(width, height, reinterpret_cast<uint8*>(&pixels[0]));
	}

	return 0;
}

