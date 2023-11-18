#ifndef __AUDIORECORDER_H__
#define __AUDIORECORDER_H__

#include "audio_types.h"

#include <vector>
#include <string>

#include <alsa/asoundlib.h>

class AudioRecorder
{
public:
    static const char *helpStr;

    AudioRecorder();
    AudioRecorder(int argc, char **argv);
    ~AudioRecorder();

    std::vector<std::string> getAudioDevsList();

    std::string getCaptureDevId() { return captureDevIdStr; }
    u_int getChannelsNumber() { return chansNumber; }
    float getGainFactor() { return gainFactor; }
    std::string getOutFile() { return outFileStr; }
    u_int getSampleRate() { return sampleRate; }
    u_int getTimeToRec() { return timeToRec; }

    bool isInited() { return inited; }
    bool record();
    bool setParameters(const std::string &capDev = "plughw:0,0", 
                        u_int chN = 1,
                        float gain = 10.5,
                        const std::string &outF = "out.bin",
                        u_int sr = 48000,
                        u_int time = 1);

private:
    // Capture parameters
    std::string captureDevIdStr;
    u_int chansNumber;
    float gainFactor;
    std::string outFileStr;
    u_int sampleRate;
    u_int timeToRec;
    // Audio buffer
    snd_pcm_t *audioBuf;
    u_int bufSize;
    u_int sampleSize;

    bool inited;
    std::string errStr;
    wav_header_t wavHeader;
    bool verbose;

    int  abufHandleError(int res);
    bool createAudioBuf();
    bool getDeviceName(std::string &deviceName, snd_ctl_t *sndCardHandler = NULL, int deviceIndex = -1, bool playback = true);
    bool getSoundCardInfo(std::string &soundCardInfo, int soundCardIndex = -1);

    std::string getLastErrorInfo();
    void init(int argc, char **argv);
    void stringToInt(char *str, unsigned int *pIntValue);
    bool validateParams();
    void wavHeaderInit();
};

#endif  // __AUDIORECORDER_H__
