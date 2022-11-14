#pragma once

#include <string>
#include <stdint.h>
#include <unistd.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <cell/sysmodule.h>
#include <sys/ppu_thread.h>
#include <cell/voice.h>

#include "Enums.hpp"
#include "Game.hpp"

#include "../Util/Exports.hpp"
#include <vector>
#include "File.h"
#include "Util/TimeHelpers.hpp"

typedef uint32_t audio_port;

extern bool g_unloadModule;

#define AUDIO_PATH "/dev_hdd0/tmp/audio/"

struct wave_header
{
    struct riff_chunk
    {
        unsigned int chunk_id;
        unsigned int chunk_size;
        unsigned int format;
    }riff;

    struct fmt_chunk
    {
        unsigned int     chunk_id;
        unsigned int     chunk_size;
        signed short     audio_format;
        unsigned short   num_channels;
        unsigned int     sample_rate;
        unsigned int     byte_rate;
        unsigned short   block_align;
        unsigned short   bits_per_sample;
    }fmt;

    struct data_chunk
    {
        unsigned int     chunk_id;
        unsigned int     chunk_size;
    }data;
};

class audio_file
{
public:
    audio_file() = default;
    audio_file(std::string name) : Name(name) { }

    std::string Name;
    wave_header Header;
    std::filesystem Handle;
    char StreamBuffer[4096];

    int StreamOffset;
    int StreamCount;
    int StreamSize;
};

enum audio_state
{
    inactive,
    ready,
    running,
    finished,
};

class Audio
{
public:
    std::vector<audio_file> Files;
    std::string Name;
    bool PlayNew;
    CellVoicePortParam VoiceParams;
    CellVoicePortParam PcmParams;
    audio_port VoicePort;
    audio_port PcmPort;
    float Volume;

    bool StartAudio();
    void StopAudio();

    void SetVolume(float volume);

    bool CreateVoicePort();
    bool CreatePcmPort();

    void ConnectPorts();

    void Add(std::string name);
    void Play(std::string name);
    void StopPlaying();

    void PlayPort(audio_port port);
    void PausePort(audio_port port);
    void DeletePort(audio_port port);

    void ClearAudio();

    void Update();

    static void sound_thread(uint64_t value);

    static void Start();
    static void Stop();
};

extern Audio GAudio;