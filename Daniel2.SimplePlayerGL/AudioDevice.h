#pragma once

// Cinecoder
#include <Cinecoder_h.h>

// License
#include "../common/cinecoder_license_string.h"

#if defined(__WIN32__)
#include <Al/al.h>
#include <Al/alc.h>
#elif defined(__APPLE__)
#include <OpenAL/OpenAL.h>
#elif defined(__LINUX__)
#include <AL/al.h>
#include <AL/alc.h>
#endif

#ifndef NDEBUG
#define __al { \
	ALuint alRes = alGetError(); \
	if (alRes != AL_NO_ERROR) { \
	printf("al error = 0x%x line = %d\n", alRes, __LINE__); \
	return alRes; \
	} }

#define __al_void { \
	ALuint alRes = alGetError(); \
	if (alRes != AL_NO_ERROR) { \
	printf("al error = 0x%x line = %d\n", alRes, __LINE__); \
	} }
#else
#define __al
#define __al_void
#endif


class AudioDevice
{
private:
    ALCdevice *device = nullptr;
    ALCcontext *context = nullptr;

public:
    AudioDevice();
    ~AudioDevice();

public:
    int Init();

private:    
    int Destroy();
    int PrintVersion();
};

