#include "stdafx.h"
#include "ReadFileDN2.h"

#define BUFFERED_FRAMES 5

ReadFileDN2::ReadFileDN2() :
	m_bProcess(false),
	m_bReadFile(false),
	m_bSeek(false),
    m_bPause(false),
    m_bLoop(false),
	m_iSpeed(1)
{
#ifdef __FILE_READ__
	m_file = nullptr;
#endif
}

ReadFileDN2::~ReadFileDN2()
{
	StopPipe();

	CloseFile();

	m_queueFrames.Free();
	m_queueFrames_free.Free();

	m_listFrames.clear();
}

int ReadFileDN2::OpenFile(const char* filename)
{
	CloseFile();

	////////////////////////////

#ifdef __STD_READ__
	m_file.open(filename, std::ofstream::in | std::ifstream::binary);

	if (!m_file.is_open())
		return -1;
#elif __FILE_READ__
#if defined(__WIN32__)
	fopen_s(&m_file, filename, "rb");
#else
	m_file = fopen(filename, "rb");
#endif
	if (!m_file)
		return -1;
#elif __UNBUFF_READ__
	if (m_file.OpenFile(filename, true) != 0)
		return -1;
#endif

	////////////////////////////

	HRESULT hr = S_OK;

	com_ptr<ICC_ClassFactory> piFactory;

	Cinecoder_CreateClassFactory((ICC_ClassFactory**)&piFactory);
	if (FAILED(hr)) return hr;

	if (SUCCEEDED(hr)) hr = piFactory->CreateInstance(CLSID_CC_MvxFile, IID_ICC_MvxFile, (IUnknown**)&m_fileMvx);

#if defined(__WIN32__)
	CC_STRING file_name_str = _com_util::ConvertStringToBSTR(filename);
#elif defined(__APPLE__) || defined(__LINUX__)
	CC_STRING file_name_str = const_cast<CC_STRING>(filename);
#endif

	if (SUCCEEDED(hr)) hr = m_fileMvx->Open(file_name_str);

	if (!SUCCEEDED(hr))
		return -1;

	CC_UINT lenght = 0;
	hr = m_fileMvx->get_Length(&lenght);

    // Reduce by one here so we don't have to do it everywhere where the frame count is needed
	m_frames = (CC_UINT)lenght - 1;

	////////////////////////////

	size_t iCountFrames = BUFFERED_FRAMES;

	for (size_t i = 0; i < iCountFrames; i++)
	{
		m_listFrames.emplace_back(CodedFrame());
		m_queueFrames_free.Queue(&m_listFrames.back());
	}

	return 0;
}

int ReadFileDN2::CloseFile()
{
	if (m_fileMvx)
		m_fileMvx->Close();

#ifdef __STD_READ__
	m_file.close();
#elif __FILE_READ__
	if (m_file) 
		fclose(m_file);
#elif __UNBUFF_READ__
	if (m_file.isValid())
		m_file.CloseFile();
#endif
	return 0;
}

int ReadFileDN2::StartPipe()
{
	Create();

	return 0;
}

int ReadFileDN2::StopPipe()
{
	m_bProcess = false;
	m_bReadFile = false;

	Close();

	return 0;
}

int ReadFileDN2::ReadFrame(size_t frame, C_Buffer & buffer, size_t & size)
{
	C_AutoLock lock(&m_critical_read);

	if (frame > m_frames)
		return -1;

#ifdef __STD_READ__
	if (m_file.is_open())
#elif __FILE_READ__
	if (m_file)
#elif __UNBUFF_READ__
	if (m_file.isValid())
#endif
	{
		CC_MVX_ENTRY Idx;
		if (!SUCCEEDED(m_fileMvx->FindEntryByCodingNumber((CC_UINT)frame, &Idx)))
			return -1;

		size_t offset = (size_t)Idx.Offset;
		size = Idx.Size;

#ifdef __STD_READ__
		buffer.Resize(size);
		m_file.seekg(offset, m_file.beg);
		m_file.read((char*)buffer.GetPtr(), size);
#elif __FILE_READ__
		buffer.Resize(size);
		_fseeki64(m_file, offset, SEEK_SET);
		fread(buffer.GetPtr(), size, 1, m_file);
#elif __UNBUFF_READ__
		DWORD rcb = 0;
		size_t new_offset = offset & ~4095;
		size_t diff = (offset - new_offset);
		
		DWORD dwsize = ((size + diff) + 4095) & ~4095;
		buffer.Resize((size_t)dwsize);

		m_file.SetFilePos(new_offset); 
		m_file.ReadFile(buffer.GetPtr(0), dwsize, &rcb);

		buffer.SetDiff(diff);
#endif
		return 0;
	}

	return -1;
}

CodedFrame* ReadFileDN2::MapFrame()
{
	C_AutoLock lock(&m_critical_queue);

	CodedFrame *pFrame = nullptr;

    m_queueFrames.Get(&pFrame, m_evExit);
	//m_queueFrames.SoftGet(&pFrame);

	return pFrame;
}

void ReadFileDN2::UnmapFrame(CodedFrame* pFrame)
{
	C_AutoLock lock(&m_critical_queue);

	if (pFrame)
	{
		m_queueFrames_free.Queue(pFrame);
	}
}

long ReadFileDN2::ThreadProc()
{
	size_t iCurEncodedFrame = 0;

	m_bProcess = true;
	m_bReadFile = true;

	data_rate = 0;

	int res = 0;
	bool bSeek = false;

	while (m_bProcess)
	{
        if (m_bPause && !m_bSeek && !bSeek)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }        

		CodedFrame* frame = nullptr;
		m_queueFrames_free.Get(&frame, m_evExit);

		if (frame)
		{
			res = 0;

			if (m_bReadFile)
			{
				res = ReadFrame(iCurEncodedFrame, frame->coded_frame, frame->coded_frame_size);
				data_rate += frame->coded_frame_size;
			}
			
            if (res != 0)
            {
                assert(0);
                printf("ReadFrame failed res=%d coded_frame_size=%zu coded_frame=%p\n", res, frame->coded_frame_size, frame->coded_frame.GetPtr());
            }
            else 
            {
                frame->flags = 0;
                if (bSeek) 
                { 
                    frame->flags = 1; 
                    bSeek = false;
                }
                frame->frame_number = iCurEncodedFrame;
                m_queueFrames.Queue(frame);
            }
		}

		iCurEncodedFrame += m_iSpeed;

        if (iCurEncodedFrame > m_frames) 
        {
            iCurEncodedFrame = 0;
            if (!m_bLoop)
                m_bPause = true;
        }
		else if (iCurEncodedFrame < 0)
			iCurEncodedFrame = m_frames;

        if (m_bSeek)
        {
            {
                C_AutoLock lock(&m_critical_queue);
                while (!m_queueFrames.Empty())
                {
                    CodedFrame *pFrame = nullptr;
                    m_queueFrames.Get(&pFrame, m_evExit);
                    if (pFrame)
                        m_queueFrames_free.Queue(pFrame);
                }
                assert(m_queueFrames.Empty());
            }

            iCurEncodedFrame = m_iSeekFrame;
            m_bSeek = false;
            bSeek = true;
        }
	}

	return 0;
}

