#ifndef __AUDIORECORDER_H__
#define __AUDIORECORDER_H__

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
    typedef union WAV_HEADER
    {
        struct {
            // WAV-формат начинается с RIFF-заголовка:

            // Содержит символы "RIFF" в ASCII кодировке
            // (0x52494646 в big-endian представлении)
            char chunkId[4];

            // 36 + subchunk2Size, или более точно:
            // 4 + (8 + subchunk1Size) + (8 + subchunk2Size)
            // Это оставшийся размер цепочки, начиная с этой позиции.
            // Иначе говоря, это размер файла - 8, то есть,
            // исключены поля chunkId и chunkSize.
            unsigned int chunkSize;

            // Содержит символы "WAVE"
            // (0x57415645 в big-endian представлении)
            char format[4];

            // Формат "WAVE" состоит из двух подцепочек: "fmt " и "data":
            // Подцепочка "fmt " описывает формат звуковых данных:

            // Содержит символы "fmt "
            // (0x666d7420 в big-endian представлении)
            char subchunk1Id[4];

            // 16 для формата PCM.
            // Это оставшийся размер подцепочки, начиная с этой позиции.
            unsigned int subchunk1Size;

            // Аудио формат, полный список можно получить здесь http://audiocoding.ru/wav_formats.txt
            // Для PCM = 1 (то есть, Линейное квантование).
            // Значения, отличающиеся от 1, обозначают некоторый формат сжатия.
            unsigned short audioFormat;

            // Количество каналов. Моно = 1, Стерео = 2 и т.д.
            unsigned short numChannels;

            // Частота дискретизации. 8000 Гц, 44100 Гц и т.д.
            unsigned int sampleRate;

            // sampleRate * numChannels * bitsPerSample/8
            unsigned int byteRate;

            // numChannels * bitsPerSample/8
            // Количество байт для одного сэмпла, включая все каналы.
            unsigned short blockAlign;

            // Так называемая "глубиная" или точность звучания. 8 бит, 16 бит и т.д.
            unsigned short bitsPerSample;

            // Подцепочка "data" содержит аудио-данные и их размер.

            // Содержит символы "data"
            // (0x64617461 в big-endian представлении)
            char subchunk2Id[4];

            // numSamples * numChannels * bitsPerSample/8
            // Количество байт в области данных.
            unsigned int subchunk2Size;

            // Далее следуют непосредственно Wav данные.
        } fields;

        char data[sizeof(fields)];
    } wav_header_t;


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
    u_int frameSize;

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
