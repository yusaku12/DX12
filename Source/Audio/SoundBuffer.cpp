#include "pch.h"
#include "SoundBuffer.h"

bool SoundBuffer::loadWav(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    char id[4];
    DWORD size;

    file.read(id, 4);                 // RIFF
    file.ignore(4);                   // size
    file.read(id, 4);                 // WAVE

    bool fmtFound = false;
    bool dataFound = false;

    while (file.read(id, 4))
    {
        file.read(reinterpret_cast<char*>(&size), 4);

        if (strncmp(id, "fmt ", 4) == 0)
        {
            // fmtチャンクを一旦バッファへ
            std::vector<char> fmt(size);
            file.read(fmt.data(), size);

            WAVEFORMATEX* wf = reinterpret_cast<WAVEFORMATEX*>(fmt.data());

            // PCMのみ許可
            if (wf->wFormatTag != WAVE_FORMAT_PCM)
                return false;

            m_format = {};
            m_format.wFormatTag = WAVE_FORMAT_PCM;
            m_format.nChannels = wf->nChannels;
            m_format.nSamplesPerSec = wf->nSamplesPerSec;
            m_format.wBitsPerSample = wf->wBitsPerSample;
            m_format.nBlockAlign = (wf->nChannels * wf->wBitsPerSample) / 8;
            m_format.nAvgBytesPerSec = m_format.nBlockAlign * m_format.nSamplesPerSec;
            m_format.cbSize = 0;

            fmtFound = true;
        }
        else if (strncmp(id, "data", 4) == 0)
        {
            m_data.resize(size);
            file.read(reinterpret_cast<char*>(m_data.data()), size);
            dataFound = true;
        }
        else
        {
            file.ignore(size);
        }

        if (fmtFound && dataFound)
            break;
    }

    return fmtFound && dataFound;
}