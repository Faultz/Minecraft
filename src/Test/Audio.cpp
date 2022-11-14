#include "Audio.h"

bool g_unloadModule;

Audio GAudio;
void sysutilCallback(uint64_t uiStatus, uint64_t uiParam __attribute__((unused)), void* pUserData)
{
    // switch (uiStatus)
    // {
    //     case CELL_SYSUTIL_REQUEST_EXITGAME:
    //         printf("sysutil callback: EXITGAME is received.\n");
    //         *((bool*)pUserData) = true;
    //         break;
    //     default:
    //         break;
    // }
}

void sys_audio_thread(uint64_t /**/)
{
    while (!g_unloadModule)
    {
        //int err = cellSysutilCheckCallback();

        GAudio.Update();

        sys_timer_usleep(10000);
    }
    sys_ppu_thread_exit(0);
}

bool Audio::StartAudio()
{
    int Result;

    Result = cellSysmoduleLoadModule(CELL_SYSMODULE_VOICE);
    if (Result != CELL_OK)
    {
        printf("failed to load voice lib\n");
        return false;
    }

    CellVoiceInitParam Params;
    memset(&Params, 0, sizeof(Params));
    Result = cellVoiceInit(&Params);
    if (Result != CELL_OK)
    {
        printf("cellVoiceInit failed\n");
        return false;
    }

    Result = cellVoiceStart();
    if (Result != CELL_OK)
    {
        printf("cellVoiceStart failed\n");
        cellVoiceEnd();
        return false;
    }

    return true;
}

void Audio::StopAudio()
{
    ClearAudio();

    DeletePort(VoicePort);
    DeletePort(PcmPort);

    cellSysmoduleUnloadModule(CELL_SYSMODULE_VOICE);

    // cellSysutilUnregisterCallback(0);
}

void Audio::SetVolume(float volume)
{
    Volume = volume;
    cellVoiceSetVolume(PcmPort, volume);
}

bool Audio::CreateVoicePort()
{
    VoiceParams.portType = CELLVOICE_PORTTYPE_OUT_SECONDARY;
    VoiceParams.bMute = false;
    VoiceParams.threshold = 100;
    VoiceParams.volume = 1.0f;
    VoiceParams.device.playerId = 0;

    int err = cellVoiceCreatePort(&VoicePort, &VoiceParams);
    if (err == CELL_OK)
        return true;
    else
    {
        printf("cellVoiceCreatePort voice_port failed\n");
        DeletePort(VoicePort);
        return false;
    }
}

bool Audio::CreatePcmPort()
{
    PcmParams.portType = CELLVOICE_PORTTYPE_IN_PCMAUDIO;
    PcmParams.bMute = false;
    PcmParams.threshold = 0;
    PcmParams.volume = 1.0f;
    PcmParams.pcmaudio.format.sampleRate = CELLVOICE_SAMPLINGRATE_16000;
    PcmParams.pcmaudio.format.dataType = CELLVOICE_PCM_INTEGER_LITTLE_ENDIAN;
    PcmParams.pcmaudio.bufSize = 4096;
    int err = cellVoiceCreatePort(&PcmPort, &PcmParams);
    if (err == CELL_OK)
        return true;
    else
    {
        printf("cellVoiceCreatePort pcm_port failed\n");
        DeletePort(PcmPort);
        return false;
    }
}

void Audio::ConnectPorts()
{
    int err = cellVoiceConnectIPortToOPort(PcmPort, VoicePort);
    if (err != CELL_OK)
    {
        printf("cellVoiceConnectIPortToOPort pcm_port to headset failed %X\n", err);
        return;
    }
}

void Audio::Update()
{
    for (int i = 0; i < Files.size();)
    {
        bool is_end_reached = false;

        audio_file& file = Files[i];

        CellVoiceBasePortInfo port;
        memset(&port, 0, sizeof(port));
        int err = cellVoiceGetPortInfo(PcmPort, &port);
        if (err != CELL_OK && err != CELL_VOICE_ERROR_SERVICE_DETACHED)
        {
            printf("cellVoiceGetPortInfo PCMInputPort failed %x\n", err);
        }

        uint32_t dataSize = file.StreamSize - sizeof(wave_header);
        uint32_t readSize = (port.numByte > 4096) ? 4096 : port.numByte;
        if ((file.StreamOffset + readSize) >= dataSize)
        {
            readSize = (dataSize - file.StreamCount);
            is_end_reached = true;
        }

        uint32_t bytes_read = file.Handle.read_off(sizeof(wave_header) + file.StreamOffset, file.StreamBuffer, readSize, 1);

        err = cellVoiceWriteToIPort(PcmPort, file.StreamBuffer, &bytes_read);
        if (err != CELL_OK)
        {
            printf("cellVoiceWriteToIPort PCMInPort failed = %0x\n", err);
        }

        file.StreamOffset += readSize;
        file.StreamCount += readSize;

        if (is_end_reached)
        {
            file.StreamOffset = 0;
            file.StreamCount = 0;
            file.Name.clear();
            Files.erase(Files.begin() + i);
            i++;
        }
    }
}

void Audio::Add(std::string name)
{
    audio_file file;
    file.Name = name;
    file.Handle = std::filesystem(name.data(), "rb");
    file.Handle.read(&file.Header, sizeof(wave_header), 1);
    file.Handle.seek(0, SEEK_END);
    file.StreamSize = file.Handle.tell() - sizeof(wave_header);

    Files.push_back(file);
}

void Audio::sound_thread(uint64_t value)
{
    while (!g_unloadModule)
    {
        if (GAudio.PlayNew)
        {
            if (GAudio.Name.size() != 0)
            {
                std::string& name = GAudio.Name;

                std::filesystem file(name.data(), "rb");
                file.seek(0, SEEK_END);

                int current_stream = 0x2C;
                int stream_count = 0;
                int end_stream = file.size() - 0x2C;
                char buffer[2048];
                memset(buffer, 0, 2048);

                GAudio.PlayNew = false;

                GAudio.SetVolume(GAudio.Volume);

                while (current_stream < end_stream)
                {
                    bool is_end_reached = false;

                    if (GAudio.PlayNew)
                        is_end_reached = true;

                    if (g_unloadModule)
                        break;

                    CellVoiceBasePortInfo port;
                    memset(&port, 0, sizeof(port));
                    int err = cellVoiceGetPortInfo(GAudio.PcmPort, &port);
                    if (err != CELL_OK && err != CELL_VOICE_ERROR_SERVICE_DETACHED)
                    {
                        printf("cellVoiceGetPortInfo PCMInputPort failed %x\n", err);
                    }

                    uint32_t dataSize = end_stream;
                    uint32_t readSize = (port.numByte < 2048) ? port.numByte : 2048;
                    if ((current_stream + readSize) >= dataSize)
                    {
                        readSize = (dataSize - stream_count);
                        is_end_reached = true;
                    }

                    uint32_t bytes_read = file.read_off(current_stream, buffer, readSize, 1);

                    err = cellVoiceWriteToIPort(GAudio.PcmPort, buffer, &bytes_read);
                    if (err != CELL_OK)
                    {
                        printf("cellVoiceWriteToIPort PCMInPort failed = %0x\n", err);
                    }

                    current_stream += readSize;
                    stream_count += readSize;

                    if (is_end_reached)
                    {
                        GAudio.SetVolume(0.f);
                        current_stream = 0;
                        stream_count = 0;
                        break;
                    }

                    sys_timer_usleep(5);
                }

                GAudio.SetVolume(GAudio.Volume);
                printf("playing new track\n");

                file.close();
            }
        }
        sleep_for(10);
    }

    sys_ppu_thread_exit(0);
}

void Audio::Play(std::string name)
{
    PlayNew = true;
    this->Name = name;
    PlayPort(PcmPort);
}

void Audio::StopPlaying()
{
    PlayNew = true;
    this->Name.clear();
    PausePort(PcmPort);
}

void Audio::PlayPort(audio_port port)
{
    if (port != CELLVOICE_INVALID_PORT_ID)
    {
        int err = cellVoiceResumePort(port);
        if (err != CELL_OK)
        {
            printf("cellVoiceResumePort failed %X\n", err);
        }
    }
}

void Audio::PausePort(audio_port port)
{
    if (port != CELLVOICE_INVALID_PORT_ID)
    {
        int err = cellVoicePausePort(port);
        if (err != CELL_OK)
        {
            printf("cellVoicePausePort failed %X\n", err);
        }
    }
}

void Audio::DeletePort(audio_port port)
{
    if (port != CELLVOICE_INVALID_PORT_ID)
    {
        int err = cellVoiceDeletePort(port);
        if (err != CELL_OK)
        {
            printf("cellVoiceDeletePort failed %X\n", err);
        }
    }
}

void Audio::ClearAudio()
{
    for (int i = 0; i < Files.size(); i++)
    {
        Files[i].Name.clear();
        memset(&Files[i], 0, sizeof(audio_file));
    }

    Files.clear();
}

void Audio::Start()
{
    if (!GAudio.StartAudio())
        return;

    GAudio.CreateVoicePort();
    GAudio.CreatePcmPort();

    GAudio.ConnectPorts();

    sys_ppu_thread_t thread;
    sys_ppu_thread_create(&thread, sound_thread, 0, 0x7C4, 0x100, 0, "song");
    sys_ppu_thread_create(&thread, sys_audio_thread, 0, 0x7CE, 0x200, 0, "bonk");
}

void Audio::Stop()
{
    GAudio.StopAudio();
}