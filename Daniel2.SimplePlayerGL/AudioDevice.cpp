#include "stdafx.h"
#include "AudioDevice.h"

AudioDevice::AudioDevice()
{
    Init();
}

AudioDevice::~AudioDevice()
{
    Destroy();
}

int AudioDevice::Init()
{
    //list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
    if (device)
        return 0;

    device = alcOpenDevice(NULL);
    //device = alcOpenDevice((ALchar*)"DirectSound3D");

    context = alcCreateContext(device, NULL);
    if (!alcMakeContextCurrent(context))
        return -1;

    alGetError(); /* clear error */

    ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
    alListener3f(AL_POSITION, 0, 0, 1.0f); __al
    alListener3f(AL_VELOCITY, 0, 0, 0); __al
    alListenerfv(AL_ORIENTATION, listenerOri); __al

    PrintVersion();
    return 0;
}

int AudioDevice::Destroy()
{
    device = alcGetContextsDevice(context); __al
    alcMakeContextCurrent(NULL); __al
    alcDestroyContext(context); __al
    alcCloseDevice(device); __al

    return 0;
}

int AudioDevice::PrintVersion()
{
    if (!device)
        return -1;

    static const ALchar alVendor[] = "OpenAL Community";
    static const ALchar alVersion[] = "1.1 ALSOFT ";
    static const ALchar alRenderer[] = "OpenAL Soft";

    const ALCchar* _alVendor = alcGetString(device, AL_VENDOR); __al
        const ALCchar* _alVersion = alcGetString(device, AL_VERSION); __al
        const ALCchar* _alRenderer = alcGetString(device, AL_RENDERER); __al

        printf("OpenAL vendor : %s\n", _alVendor == nullptr ? alVendor : _alVendor);
    printf("OpenAL renderer : %s\n", _alVersion == nullptr ? alVersion : _alVersion);
    printf("OpenAL version : %s\n", _alRenderer == nullptr ? alRenderer : _alRenderer);

    printf("-------------------------------------\n");

    return 0;
}

