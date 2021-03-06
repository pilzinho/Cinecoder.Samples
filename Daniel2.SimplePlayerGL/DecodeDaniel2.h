#pragma once

#include "Block.h"
#include "ReadFileDN2.h"

enum IMAGE_FORMAT { IMAGE_FORMAT_UNKNOWN, IMAGE_FORMAT_RGBA8BIT, IMAGE_FORMAT_BGRA8BIT, IMAGE_FORMAT_RGBA16BIT, IMAGE_FORMAT_BGRA16BIT, IMAGE_FORMAT_RGB30 };
enum BUFFER_FORMAT { BUFFER_FORMAT_UNKNOWN, BUFFER_FORMAT_RGBA32, BUFFER_FORMAT_RGBA64, BUFFER_FORMAT_YUY2, BUFFER_FORMAT_Y216, BUFFER_FORMAT_NV12, BUFFER_FORMAT_P016 };

enum VIDEO_DECODER_TYPE 
{ 
	VD_TYPE_UNKNOWN = -1,
	
	VD_TYPE_CPU = 0,		// CPU
	VD_TYPE_CUDA,			// NVIVIA GPU(CUDA)
	VD_TYPE_QuickSync,		// Intel GPU(QuickSync)
	VD_TYPE_OpenCL			// OpenCL
};

struct ST_VIDEO_DECODER_PARAMS
{
	size_t max_count_decoders;
	VIDEO_DECODER_TYPE type;
	CC_VDEC_SCALE_FACTOR scale_factor;
	IMAGE_FORMAT outputFormat;

	ST_VIDEO_DECODER_PARAMS()
	{
		max_count_decoders = 2;
		scale_factor = CC_VDEC_NO_SCALE;
		outputFormat = IMAGE_FORMAT_UNKNOWN;
		type = VD_TYPE_CPU;
	}
};

#if defined(__WIN32__)
class GPURenderDX;
#endif

std::string GetCinecoderVersion();

class DecodeDaniel2 : public C_SimpleThread<DecodeDaniel2>, public ICC_DataReadyCallback
{
private:
	std::string m_filename;

	size_t m_width;
	size_t m_height;
	size_t m_stride;
	IMAGE_FORMAT m_outputImageFormat;
	BUFFER_FORMAT m_outputBufferFormat;
	CC_COLOR_FMT m_fmt;
	const char* m_strStreamType;

	CC_FRAME_RATE m_FrameRate;
	CC_CHROMA_FORMAT m_ChromaFormat;
	DWORD m_BitDepth;

	ST_VIDEO_DECODER_PARAMS m_dec_params;

	bool m_bProcess;
	bool m_bPause;
    bool m_bLoop;
    std::atomic_bool m_bSeek;
	bool m_bDecode;
	bool m_bInitDecoder;
	bool m_bUseCuda;
	bool m_bUseCudaHost;
	bool m_bPutColorFormat;
	bool m_bForce8Bit;
    size_t m_iSeekFrame = 0;
    int frameReadingErrors = 0;
    int frameProcessingErrors = 0;

	ReadFileDN2 m_file;

	C_QueueT<C_Block> m_queueFrames;
	C_QueueT<C_Block> m_queueFrames_free;

	C_Event	m_eventInitDecoder;

	std::list<C_Block> m_listBlocks;

	com_ptr<ICC_ClassFactory> m_piFactory;
	com_ptr<ICC_VideoDecoder> m_pVideoDec;

	com_ptr<ICC_MediaReader> m_pMediaReader;

	ULONGLONG m_llDuration;
	ULONGLONG m_llTimeBase;
	bool bIntraFormat;
	bool bCalculatePTS;

	LONGLONG m_iNegativePTS;

public:
	DecodeDaniel2();
	~DecodeDaniel2();
	
public:
	int OpenFile(const char* const filename, ST_VIDEO_DECODER_PARAMS dec_params);
	int StartDecode();
	int StopDecode();

	size_t GetImageWidth() { return m_width; }
	size_t GetImageHeight() { return m_height; }
	IMAGE_FORMAT GetImageFormat() { return m_outputImageFormat; }
	BUFFER_FORMAT GetBufferFormat() { return m_outputBufferFormat; }

	C_Block* MapFrame();
	void  UnmapFrame(C_Block* pBlock);

	bool isProcess() { return m_bProcess; }
	bool isPause() { return m_bPause; }
	bool isDecode() { return m_bDecode; }

    void SetPause(bool bPause) { m_bPause = bPause; m_file.SetPause(bPause); }
	void SetDecode(bool bDecode) { m_bDecode = bDecode; }

    void SetLoop(bool bLoop) { m_bLoop = bLoop; m_file.SetLoop(bLoop); }

    size_t SeekFrame(size_t nFrame);

	void SetSpeed(int iSpeed) { if (bIntraFormat) m_file.SetSpeed(iSpeed); }
	int GetSpeed() { return m_file.GetSpeed(); }

	void SetReadFile(bool bReadFile) { if (bIntraFormat) m_file.SetReadFile(bReadFile); }
	bool GetReadFile() { return m_file.GetReadFile(); }

	size_t GetCountFrames() { return m_file.GetCountFrames(); }
	size_t GetDataRate(bool bClearData) { return m_file.GetDataRate(bClearData); }

	double GetFrameRate() { return ((double)m_FrameRate.num / (double)m_FrameRate.denom); }
	CC_FRAME_RATE GetFrameRateValue() { return m_FrameRate; }
    CC_CHROMA_FORMAT GetChromaFormat() { return m_ChromaFormat; }
    CC_COLOR_FMT GetColorFormat() { return m_fmt; }
    DWORD GetBitDepth() { return m_BitDepth; }

    void SetForce8Bit(const bool force8Bit) { m_bForce8Bit = force8Bit; }

    void SetErrorHandler(ICC_ErrorHandler* const handler);

#if defined(__WIN32__)
private:
	GPURenderDX* m_pRender;
	IDXGIAdapter1* m_pCapableAdapter;
	com_ptr<ICC_D3D11VideoProducer> m_pVideoDecD3D11;
    ID3D11Device* m_pd3dDevice;
    ID3D10Multithread* m_pMultithread;
    bool m_bD3D11AccEnabled = false;

public:
	void InitD3DX11Render(GPURenderDX *pRender) { m_pRender = pRender; }
	void InitD3DXAdapter(IDXGIAdapter1* pCapableAdapter) { m_pCapableAdapter = pCapableAdapter; }
	bool IsD3DX11Acc() { return m_pVideoDecD3D11 ? true : false; }

    void SetD3DDevice(ID3D11Device* const device) { m_pd3dDevice = device; }
    void SetMultithread(ID3D10Multithread* multithread) { m_pMultithread = multithread; }
    void EnableD3D11Accel(const bool enable) { m_bD3D11AccEnabled = enable; }

private:
	void RegisterResourceD3DX11(ID3D11Resource* pResource)
	{
		HRESULT hr = S_OK;
		
		if (pResource)
			hr = m_pVideoDecD3D11->RegisterResource(pResource); __check_hr
	}
	void UnregisterResourceD3DX11(ID3D11Resource* pResource)
	{
		HRESULT hr = S_OK;

		if (pResource)
			hr = m_pVideoDecD3D11->UnregisterResource(pResource); __check_hr
	}

    HRESULT CreateD3DXBuffer(ID3D11Buffer** pBuffer, size_t iSizeBuffer);
#endif

private:
	int CreateDecoder();
	int DestroyDecoder();

	int InitValues();
	int DestroyValues();

    void Destroy();

#if defined(__WIN32__)
	HRESULT LoadPlugin(const char* pluginDLL);
#endif

private:
	virtual ULONG STDMETHODCALLTYPE AddRef(void) { return 2; }
	virtual ULONG STDMETHODCALLTYPE Release(void) { return 1; }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv)
	{
		if (riid == IID_IUnknown || riid == IID_ICC_DataReadyCallback) { *ppv = (ICC_DataReadyCallback *)this; return S_OK; }
		return E_NOINTERFACE;
	}
	virtual HRESULT STDMETHODCALLTYPE DataReady(IUnknown *pDataProducer);

private:
	friend class C_SimpleThread<DecodeDaniel2>;
	long ThreadProc();
};
