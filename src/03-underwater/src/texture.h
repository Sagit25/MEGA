#ifndef TEXTURE_H
#define TEXTURE_H
#define STB_IMAGE_IMPLEMENTATION   // use of stb functions once and for all
#include "stb_image.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Texture{
public:
    unsigned int ID;
    int width;
    int height;
    int channels;
    std::string path; // Assimp 텍스처 캐싱을 위해 추가된 경로 변수

    Texture() {}
    Texture(const char* filePath)
    {   
        path = std::string(filePath);
        glGenTextures(1, &ID);
        glBindTexture(GL_TEXTURE_2D, ID);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_set_flip_vertically_on_load(true);
        
        unsigned char *data = stbi_load(filePath, &width, &height, &channels, 0);
        if (data)
        {   
            if(channels == 3){
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            }else{
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            }
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else
        {
            std::cout << "Failed to load texture: " << filePath << std::endl;
        }
        stbi_image_free(data);
    }
};

class DepthMapTexture 
{
public:
	unsigned int ID;
	unsigned int depthMapFBO;
	int width;
	int height;

	DepthMapTexture(int shadow_width, int shadow_height) 
	{
		width = shadow_width;
		height = shadow_height;
		glGenFramebuffers(1, &depthMapFBO);
		glGenTextures(1, &ID);
		glBindTexture(GL_TEXTURE_2D, ID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, ID, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
};

class CausticTexture
{
public:
    unsigned int ID = 0;
    int width = 0;
    int height = 0;
    int frameCount = 0;

    CausticTexture(const char* directory, int frameCount)
        : frameCount(frameCount)
    {
        std::vector<unsigned char> allFrames;

        // load datas
        for (int i = 0; i < frameCount; i++) {
            char filePath[256];
            std::snprintf(filePath, sizeof(filePath), "%s/frame%02d.png", directory, i + 1);

            int frameWidth = 0;
            int frameHeight = 0;
            int frameChannels = 0;

            stbi_set_flip_vertically_on_load(false);
            unsigned char* framePixels = stbi_load(filePath, &frameWidth, &frameHeight, &frameChannels, 1);
            
            if (!framePixels) {
                std::cout << "Failed to load texture: " << filePath << std::endl;
                stbi_image_free(framePixels);
                return;
            }

            if (i == 0) {
                width = frameWidth;
                height = frameHeight;
            }
            else if (frameWidth != width || frameHeight != height) {
                std::cout << "Caustic texture size mismatch: " << filePath << std::endl;
                stbi_image_free(framePixels);
                return;
            }

            int pixelCount = frameWidth * frameHeight;
            allFrames.insert(allFrames.end(), framePixels, framePixels + pixelCount);
            stbi_image_free(framePixels);
        }

        // normalize to emphasize caustics
        unsigned char minValue = allFrames[0];
        unsigned char maxValue = allFrames[0];
        for (int i = 1; i < allFrames.size(); i++) {
            minValue = std::min(minValue, allFrames[i]);
            maxValue = std::max(maxValue, allFrames[i]);
        }

        int range = std::max(1, (int)maxValue - (int)minValue);
        for (int i = 0; i < allFrames.size(); i++) {
            allFrames[i] = (unsigned char)(((int)allFrames[i] - (int)minValue) * 255 / range);
        }

        glGenTextures(1, &ID);
        glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, width, height, frameCount,
            0, GL_RED, GL_UNSIGNED_BYTE, allFrames.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
};

#endif
