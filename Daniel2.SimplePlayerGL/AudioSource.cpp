#include "stdafx.h"
#include "AudioSource.h"
//#include "three10_Log/PluginLogger.h"

using namespace std;
using namespace std::chrono;

using Seconds = std::chrono::duration<double>;
using Clock = std::chrono::high_resolution_clock;

#if defined(__APPLE__) || defined(__LINUX__)
typedef signed char INT8;
typedef signed short INT16;
typedef long long UINT64;
#endif

#ifndef NDEBUG
#define __al { \
	ALuint alRes = alGetError(); \
	if (alRes != AL_NO_ERROR) { \
	fprintf(stdout, "al error = 0x%x line = %d\n", alRes, __LINE__); \
	return alRes; \
	} }

#define __al_void { \
	ALuint alRes = alGetError(); \
	if (alRes != AL_NO_ERROR) { \
	fprintf(stdout, "al error = 0x%x line = %d\n", alRes, __LINE__); \
	} }
#else
#define __al
#define __al_void
#endif

static void ReverseSamples(BYTE *p, int iSize, int nBlockAlign)
{
	long lActual = iSize;

	if (lActual == 0) { assert(0); return; };
	if (nBlockAlign != 4) { assert(0); return; };

	UINT64 *p_src = (UINT64 *)(p);
	UINT64 *p_dst = ((UINT64 *)(p + lActual)) - 1;
	UINT64 temp;

	while (p_src < p_dst)
	{
		temp = *p_src;
		*p_src++ = *p_dst;
		*p_dst-- = temp;
	};
}

static void AliasingSamples(BYTE *p, int iSize, int nBlockAlign, int nChannels)
{
	long lActual = iSize;

	const long iMaxValue = 32;

	if (lActual == 0) { assert(0); return; };
	if (nBlockAlign != 4) { assert(0); return; };

	if (lActual / nBlockAlign < 2 * iMaxValue) return;

	INT16 *p_Beg = (INT16 *)(p);
	INT16 *p_End = ((INT16 *)(p + lActual)) - 1;
	float ftemp;

	for (long i = 0; i <= iMaxValue; i++)
	{
		ftemp = ((float)i / (float)iMaxValue);
		for (long ic = 0; ic < nChannels; ic++)
		{
			*p_Beg = (INT16)((float)*p_Beg * ftemp); p_Beg++;
			*p_End = (INT16)((float)*p_End * ftemp); p_End--;
		}
	}
}

static void list_audio_devices(const ALCchar *devices)
{
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	fprintf(stdout, "Devices list:\n");
	fprintf(stdout, "----------\n");
	while (device && *device != '\0' && next && *next != '\0') {
		fprintf(stdout, "%s\n", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	fprintf(stdout, "----------\n");
}

AudioSource::AudioSource() :
	m_bInitialize(false),
	source(0),
	buffers{ 0 },
	m_FrameRate{ 25, 1 },
	m_bAudioPause(false),
	m_iSpeed(1),
	m_bProcess(false),
    m_iCurrentFrame(0)
{
}

AudioSource::~AudioSource()
{
	m_bProcess = false;

	Close(); // closing thread <ThreadProc>
    DestroySource();
}

int AudioSource::Init(CC_FRAME_RATE video_framerate, const size_t frameCount)
{
	m_FrameRate = video_framerate;
    m_iFrameCount = frameCount;
	return 0;
}

int AudioSource::Play(const size_t frameNumber)
{
    ALvoid* data = nullptr;
    ALsizei size = 0;
    if (UpdateAudioChunk(frameNumber, &data, &size) == S_OK && data && size > 0)
    {
        ALsizei frequency = static_cast<ALsizei>(m_iSampleRate);
        ALenum  format = (m_iNumChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

        ALuint buffer;

        alSourceUnqueueBuffers(source, 1, &buffer); __al
        alBufferData(buffer, format, data, size, frequency); __al
        alSourceQueueBuffers(source, 1, &buffer); __al
    }
}

int AudioSource::InitSource()
{
    alGenBuffers(NUM_BUFFERS, buffers); __al
    alGenSources(1, &source); __al

    alSourcef(source, AL_PITCH, 1); __al
    alSourcef(source, AL_GAIN, 1); __al
    alSource3f(source, AL_POSITION, 0, 0, 0); __al
    alSource3f(source, AL_VELOCITY, 0, 0, 0); __al
    alSourcei(source, AL_LOOPING, AL_FALSE); __al

    for (size_t i = 0; i < NUM_BUFFERS; i++)
    {
        HRESULT hr = S_OK;
        DWORD cbRetSize = 0;

        BYTE* pb = audioChunk.data();
        DWORD cb = static_cast<DWORD>(audioChunk.size());

        hr = m_pMediaReader->GetAudioSamples(CAF_PCM16, i * m_iSampleCount, (CC_UINT)m_iSampleCount, pb, cb, &cbRetSize);
        if (SUCCEEDED(hr) && cbRetSize > 0)
        {
            ALvoid* data = pb;
            ALsizei size = static_cast<ALsizei>(cbRetSize);
            ALsizei frequency = static_cast<ALsizei>(m_iSampleRate);
            ALenum  format = (m_iNumChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

            alBufferData(buffers[i], format, data, size, frequency); __al
        }
    }

    alSourceQueueBuffers(source, NUM_BUFFERS, buffers); __al
    m_bInitialize = true;
    Create(); // creating thread <ThreadProc>
}

int AudioSource::DestroySource()
{
	alDeleteSources(1, &source); __al
	alDeleteBuffers(NUM_BUFFERS, buffers); __al

	return 0;
}

int AudioSource::OpenFile(const char* const filename, const bool autoPlay /*=true*/)
{
	HRESULT hr = S_OK;

	com_ptr<ICC_ClassFactory> piFactory;

	Cinecoder_CreateClassFactory((ICC_ClassFactory**)&piFactory); // get Factory
	if (FAILED(hr)) return hr;

	hr = piFactory->AssignLicense(COMPANYNAME, LICENSEKEY); // set license
	if (FAILED(hr)) return hr;

	hr = piFactory->CreateInstance(CLSID_CC_MediaReader, IID_ICC_MediaReader, (IUnknown**)&m_pMediaReader);
	if (FAILED(hr)) return hr;

#if defined(__WIN32__)
	CC_STRING file_name_str = _com_util::ConvertStringToBSTR(filename);
#elif defined(__APPLE__) || defined(__LINUX__)
	CC_STRING file_name_str = const_cast<CC_STRING>(filename);
#endif

	hr = m_pMediaReader->Open(file_name_str);
	if (FAILED(hr)) return hr;

	CC_INT numAudioTracks = 0;
	hr = m_pMediaReader->get_NumberOfAudioTracks(&numAudioTracks);
	if (FAILED(hr)) return hr;

	if (numAudioTracks == 0)
	{
		printf("numAudioTracks == 0\n");
		m_pMediaReader = nullptr;
		return -1;
	}

	CC_UINT iCurrentAudioTrackNumber = 0;

	hr = m_pMediaReader->put_CurrentAudioTrackNumber(iCurrentAudioTrackNumber);
	if (FAILED(hr)) return hr;

	hr = m_pMediaReader->get_CurrentAudioTrackInfo((ICC_AudioStreamInfo**)&m_pAudioStreamInfo);
	if (FAILED(hr)) return hr;

	CC_TIME Duration = 0;
	hr = m_pMediaReader->get_Duration(&Duration);
	if (FAILED(hr)) return hr;

	CC_INT FrameCount = 0;
	hr = m_pMediaReader->get_NumberOfFrames(&FrameCount);
	if (FAILED(hr)) return hr;

	CC_BITRATE BitRate;
	CC_UINT BitsPerSample;
	CC_UINT ChannelMask;
	CC_FRAME_RATE FrameRate;
	CC_UINT NumChannels;
	CC_UINT SampleRate;
	CC_ELEMENTARY_STREAM_TYPE StreamType;

	hr = m_pAudioStreamInfo->get_BitRate(&BitRate);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_BitsPerSample(&BitsPerSample);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_ChannelMask(&ChannelMask);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_FrameRate(&FrameRate);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_NumChannels(&NumChannels);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_SampleRate(&SampleRate);
	if (FAILED(hr)) return hr;

	hr = m_pAudioStreamInfo->get_StreamType(&StreamType);
	if (FAILED(hr)) return hr;

	BitsPerSample = 16; // always play in PCM16

	size_t sample_count = (SampleRate / (m_FrameRate.num / m_FrameRate.denom));
	size_t sample_bytes = sample_count * NumChannels * (BitsPerSample >> 3);

	m_iSampleCount = sample_count;
	m_iSampleRate = SampleRate;
	m_iSampleBytes = sample_bytes;
	m_iNumChannels = NumChannels;
	m_iBitsPerSample = BitsPerSample;
	m_iBlockAlign = (m_iNumChannels * m_iBitsPerSample) / 8;

	audioChunk.resize(sample_bytes);

    if (InitSource() < 0 || SetPause(!autoPlay) < 0)
        return -1;

    return 0;
}

int AudioSource::PlayFrame(size_t iFrame)
{
    //m_iCurrentFrame = iFrame;
	if (!m_bInitialize)
		return -1;

	if (!m_bAudioPause)
	{
		C_AutoLock lock(&m_CritSec);

		if (queueFrames.size() > NUM_BUFFERS)
			queueFrames.pop();

		queueFrames.push(iFrame);
	}

	return 0;
}

int AudioSource::SetPause(bool bPause)
{
	if (!m_bInitialize)
		return -1;

	m_bAudioPause = bPause;

    if (!m_bAudioPause) {
        alSourcePlay(source); __al
    }
    else {
        alSourcePause(source); __al
    }

	return 0;
}

void AudioSource::SetVolume(float fVolume)
{
    if (fVolume >= 0.f && fVolume <= 1.f)
    {
        alSourcef(source, AL_GAIN, fVolume); __al_void
    }
}

float AudioSource::GetVolume()
{
    float fVolume = 0;
    alGetSourcef(source, AL_GAIN, &fVolume); __al_void

        return fVolume;
}

long AudioSource::ThreadProc()
{
	m_bProcess = true;
    m_iCurrentFrame = NUM_BUFFERS;
    double timePerFrame = 1.0 / (m_FrameRate.num / (double)m_FrameRate.denom);
    Seconds printStatsInterval(1); // Print stats every n seconds
    auto lastPrintStatsTime = Clock::now();
    int decodedFps = 0;

	while (m_bProcess)
	{
        if (m_bAudioPause)
        {
            this_thread::sleep_for(milliseconds(10));
            continue;
        }

		ALint numProcessed = 0;
		alGetSourcei(source, AL_BUFFERS_PROCESSED, &numProcessed); __al
		if (numProcessed > 0 /*&& m_iCurrentFrame < m_iFrameCount*/)
        {
            if (queueFrames.size() > 0)
            {
                C_AutoLock lock(&m_CritSec);
                m_iCurrentFrame = queueFrames.front();
                queueFrames.pop();
            }

            ++decodedFps;
			ALvoid* data = nullptr;
			ALsizei size = 0;
            if (UpdateAudioChunk(m_iCurrentFrame/*++*/, &data, &size) == S_OK && data && size > 0)
            {
				ALsizei frequency = static_cast<ALsizei>(m_iSampleRate);
				ALenum  format = (m_iNumChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
				ALuint buffer;

                alSourceUnqueueBuffers(source, 1, &buffer); __al
				alBufferData(buffer, format, data, size, frequency); __al
                alSourceQueueBuffers(source, 1, &buffer); __al                
            }
            --numProcessed;
        }

        ALint source_state = 0;
        alGetSourcei(source, AL_SOURCE_STATE, &source_state); __al
        if (source_state != AL_PLAYING && !m_bAudioPause)
            alSourcePlay(source); __al

        auto statsDuration = duration_cast<Seconds>(Clock::now() - lastPrintStatsTime);
        if (statsDuration >= printStatsInterval)
        {
            //LogVerbose("Decoded Audio FPS: %u, Position: %u", decodedFps, m_iCurrentFrame.load());
            decodedFps = 0;
            lastPrintStatsTime = Clock::now();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
		//this_thread::sleep_for(Seconds(timePerFrame));
	}

	return 0;
}

HRESULT AudioSource::UpdateAudioChunk(size_t iFrame, ALvoid** data, ALsizei* size)
{
	HRESULT hr = S_OK;
	DWORD cbRetSize = 0;

	BYTE* pb = audioChunk.data();
	DWORD cb = static_cast<DWORD>(audioChunk.size());

	hr = m_pMediaReader->GetAudioSamples(CAF_PCM16, iFrame * m_iSampleCount, (CC_UINT)m_iSampleCount, pb, cb, &cbRetSize);
	if (SUCCEEDED(hr) && cbRetSize > 0)
	{
		// if we playing in the opposite direction we need reverse audio samples
		if (m_iSpeed < 0)
			ReverseSamples(pb, (int)cbRetSize, (int)m_iBlockAlign);

		// if we playing with speed > 1 we need aliasing our audio samples
		if (abs(m_iSpeed) > 1)
			AliasingSamples(pb, (int)cbRetSize, (int)m_iBlockAlign, (int)m_iNumChannels);

		*data = pb;
		*size = static_cast<ALsizei>(cbRetSize);
	}

	return hr;
}
