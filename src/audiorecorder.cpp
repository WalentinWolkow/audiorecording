#include "audiorecorder.h"
#include "debug.h"

#include <math.h>
#include <getopt.h>

//#include <sstream>
#include <iomanip>

// Static public members
const char *AudioRecorder::helpStr = "Usage: audiorecording [options]\n"
                                     "Options:\n"
                                     "  -C, --capture_dev   Capture device Id, for examle \"plughw:0,0\"\n"
                                     "  -c, --chans_number  Number of channels, default 1\n"
                                     "  -g, --gain          Gain factor. Must be from -40.0 to 40.0, default 10.5\n"
                                     "  -h, --help          Show help\n"
                                     "  -l, --list          Show list of all audio devices\n"
                                     "  -o, --out_file      Output file for audio data name and path\n"
                                     "  -s, --sample_rate   Sample rate\n"
                                     "  -t, --time_to_rec   Recording duration, seconds";


// Public members
AudioRecorder::AudioRecorder() :
    audioBuf(NULL),
    chansNumber(1),
    gainFactor(10.5),
    inited(false),
    sampleRate(0),
    timeToRec(0),
    verbose(false)
{
//    HERE();
    wavHeaderInit();
}

AudioRecorder::AudioRecorder(int argc, char **argv) :
    audioBuf(NULL),
    chansNumber(1),
    gainFactor(10.5),
    inited(false),
    sampleRate(0),
    timeToRec(0),
    verbose(false)
{
//    HERE();
    wavHeaderInit();
    init(argc, argv);
}

AudioRecorder::~AudioRecorder()
{
//    HERE();
    if (!audioBuf)
        return;

//    HERE();
    snd_pcm_close(audioBuf);
}


// Public methods
std::vector<std::string> AudioRecorder::getAudioDevsList()
{
    std::vector<std::string> hwInfo;

    for (int cardIndex = -1;;)
    {
        // Get next sound card index
        if (snd_card_next(&cardIndex) != 0 || cardIndex == -1)
            break;

        std::string soundCardInfo;
        if (getSoundCardInfo(soundCardInfo, cardIndex))
            hwInfo.push_back(soundCardInfo);
    }

    return hwInfo;
}

bool AudioRecorder::record()
{
    if (!audioBuf)
        return false;

    // Open file for saving data
    FILE *fdOut = fopen(outFileStr.c_str(), "w+");
    if (fdOut == NULL)
    {
        errStr = "Can not open output file: \"" + outFileStr + "\"!";
        ERR(errStr);

        return false;
    }

    // Determine if a wav-header is needed
    bool hasWavHeader;
    {
        size_t pos = outFileStr.rfind(".wav");
        hasWavHeader = pos != std::string::npos && outFileStr.size() - pos == 4;
    }

    void *dataBuf;
    size_t dataBufSize;

    if (hasWavHeader)
        dataBufSize = sizeof(wavHeader.data);
    else
        dataBufSize = sizeof(short);

    if ((dataBuf = malloc(dataBufSize)) == NULL)
    {
        fclose(fdOut);

        errStr = "Can not allocate memory space for data buffer!";
        ERR(errStr);

        return false;
    }
    dataBufSize = 0;    // force zero

    // Reserve space for WAV-header in file, if it's necessary
    if (hasWavHeader)
        fwrite((void *)dataBuf, sizeof(char), sizeof(wavHeader.data), fdOut);

    // Start streaming
    if (snd_pcm_start(audioBuf) != 0)
    {
        errStr = "snd_pcm_start(audioBuf) error!";
        ERR(errStr);

        return false;
    }

    int res = 0;
    // Read audio samples from audio buffer and write to temporary file
    for (u_int samplesCount = 0, samplesCountMax = sampleRate * timeToRec; samplesCount < samplesCountMax; )
    {
        static size_t timeExpired = 0;
        static size_t samplesPerSec = 0;

        if (res < 0)
        {
            if (abufHandleError(res) != 0)
            {
                errStr = "abufHandleError(res)";
                ERR(errStr);

                return false;
            }

            // Start streaming if necessary
            if (snd_pcm_state(audioBuf) != SND_PCM_STATE_RUNNING)
                if (snd_pcm_start(audioBuf) != 0)
                {
                    errStr = "Streaming restart error!";
                    ERR(errStr);

                    return false;
                }
        }

        // Refresh audio buffer state
        if ((res = snd_pcm_avail_update(audioBuf)) < 0)
            continue;

        // Get audio data region available for reading
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t offset;
        snd_pcm_uframes_t samples = bufSize / sampleSize;
        if ((res = snd_pcm_mmap_begin(audioBuf, &areas, &offset, &samples)) != 0)
            continue;

        if (samples == 0)
        {
            // Buffer is empty. Wait 100ms until some new data is available
            int period_ms = 100;
            usleep(period_ms * 1000);

            continue;
        }

        // Increase data buffer size if necessary
        size_t dataBlockSize = samples * sampleSize;
        if (dataBufSize < dataBlockSize)
        {
            free(dataBuf);
            if ((dataBuf = malloc(dataBlockSize)) == NULL)
            {
                fclose(fdOut);

                errStr = "Can not allocate memory space for data buffer!";
                ERR(errStr);

                return false;
            }
        }

        // Copy data from ring buffer
        {
            char *src = (char *)areas[0].addr + offset * areas[0].step / 8,
                 *dst = (char *)dataBuf;
            for (size_t i = 0; i < dataBlockSize; )
            {
                dst[i] = src[i];

                if (++i % sampleSize == 0)
                    if (++samplesPerSec == sampleRate)
                    {
                        samplesPerSec = 0;
                        ++timeExpired;
                    }
            }
        }

        samplesCount += samples;

        // Mark the data chunk as read
        res = snd_pcm_mmap_commit(audioBuf, offset, samples);
        if (res >= 0 && (snd_pcm_uframes_t)res != samples)
            // Not all samples are processed
            res = -EPIPE;

        // ...

        // Write to file
        fwrite(dataBuf, sizeof(char), dataBlockSize, fdOut);

        if (!samplesPerSec)
        {
            // ...

            if (verbose)
            {
                size_t hours = timeExpired / 3600,
                       mins = timeExpired % 3600 / 60,
                       secs = timeExpired % 60;

                std::cerr << '\r' << HIDE_CUR
                          << std::setfill('0') << std::setw(2) << hours << ':'
                          << std::setfill('0') << std::setw(2) << mins << ':'
                          << std::setfill('0') << std::setw(2) << secs;
            }
        }
    }

    if (verbose)
        std::cout << SHOW_CUR << std::endl;

    free(dataBuf);

    if (hasWavHeader)
    {
        // Prepare wav-header
        fseek(fdOut, 0, SEEK_END);
        long fileSize = ftell(fdOut);

        wavHeader.fields.chunkSize = fileSize + sizeof(wavHeader.data) - sizeof(wavHeader.fields.chunkId) - sizeof(wavHeader.fields.chunkSize);
        wavHeader.fields.numChannels = chansNumber;
        wavHeader.fields.sampleRate = sampleRate;
        wavHeader.fields.byteRate = chansNumber * sampleRate * 2;
        wavHeader.fields.blockAlign = chansNumber * 2;
        wavHeader.fields.subchunk2Size = fileSize;

        fseek(fdOut, 0, SEEK_SET);
        fwrite(wavHeader.data, sizeof(char), sizeof(wavHeader.data), fdOut);
    }

    fclose(fdOut);

    return true;
}

bool AudioRecorder::setParameters(const std::string &capDev, u_int chN, float gain, const std::string &outF, u_int sr, u_int time)
{
    inited = false;

    captureDevIdStr = capDev;
    chansNumber = chN;
    gainFactor = gain;
    outFileStr = outF;
    sampleRate = sr;
    timeToRec = time;

    if (!validateParams())
        return false;

    return inited = createAudioBuf();
}


// Private methods
int AudioRecorder::abufHandleError(int res)
{
    switch (res)
    {
        case -ESTRPIPE:
            // Sound device is temporarily unavailable.  Wait until it's online.
            while ((res = snd_pcm_resume(audioBuf)) == -EAGAIN)
            {
                int period_ms = 100;
                usleep(period_ms * 1000);
            }

            if (res == 0)
                return 0;
            // fallthrough

        case -EPIPE:
            // Overrun or underrun occurred.  Reset buffer.
            if ((res = snd_pcm_prepare(audioBuf)) < 0)
                return res;

            return 0;
    }

    return res;
}

bool AudioRecorder::createAudioBuf()
{
    // Attach audio buffer to device
    if (captureDevIdStr.empty())
    {
//        HERE();
        captureDevIdStr = "plughw:0,0";   // Use default device
    }

    if (snd_pcm_open(&audioBuf, captureDevIdStr.c_str(), SND_PCM_STREAM_CAPTURE, 0) != 0)
    {
        errStr = "Audio buffer opening error (snd_pcm_open())!";
        ERR(errStr);
        return false;
    }

    // Get device property-set
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    if (snd_pcm_hw_params_any(audioBuf, params) < 0)
    {
        errStr = "Error of getting device property set!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Specify how we want to access audio data
    if (snd_pcm_hw_params_set_access(audioBuf, params, SND_PCM_ACCESS_MMAP_INTERLEAVED) != 0)
    {
        errStr = "Audio data access specifing error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Set sample format
    if (snd_pcm_hw_params_set_format(audioBuf, params, SND_PCM_FORMAT_S16_LE) != 0)
    {
        errStr = "Sample format setting error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Set channels number
    if (snd_pcm_hw_params_set_channels_near(audioBuf, params, &chansNumber) != 0)
    {
        errStr = "Channels number setting error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Set sample rate
    if (snd_pcm_hw_params_set_rate_near(audioBuf, params, &sampleRate, 0) != 0)
    {
        errStr = "Sample rate setting error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Set audio buffer length
    u_int buffer_length_usec = 500 * 1000;
    if (snd_pcm_hw_params_set_buffer_time_near(audioBuf, params, &buffer_length_usec, NULL) != 0)
    {
        errStr = "Audio buffer length setting error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    // Apply configuration
    if (snd_pcm_hw_params(audioBuf, params) != 0)
    {
        errStr = "Audio buffer apply configuration error!";
        ERR(errStr);
        snd_pcm_close(audioBuf);
        audioBuf = NULL;
        return false;
    }

    sampleSize = (16 / 8) * chansNumber;
    bufSize = sampleRate * sampleSize * buffer_length_usec / 1000000;

    return true;
}

bool AudioRecorder::getDeviceName(std::string &deviceName, snd_ctl_t *sndCardHandler, int deviceIndex, bool playback)
{
    deviceName.clear();

    if (!sndCardHandler || deviceIndex < 0)
    {
        errStr = "Wrong handler or device index!";
        ERR(errStr << "\nsndCardHandler = " << sndCardHandler << ", deviceIndex = " << deviceIndex);
        return false;
    }

    snd_pcm_info_t *pcmInfo;
    snd_pcm_info_alloca(&pcmInfo);

    snd_pcm_info_set_device(pcmInfo, deviceIndex);
    snd_pcm_info_set_stream(pcmInfo, playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);

    if (snd_ctl_pcm_info(sndCardHandler, pcmInfo) != 0)
    {
        errStr = "snd_ctl_pcm_info(sndCardHandler, pcmInfo) error!";
//        ERR(errStr);
        return false;
    }

    deviceName = snd_pcm_info_get_name(pcmInfo);

//    DBG("Card = " << snd_pcm_info_get_card(pcmInfo));
//    DBG("Device = " << snd_pcm_info_get_device(pcmInfo));
//    DBG("Id = \"" << snd_pcm_info_get_id(pcmInfo) << '\"');

    return true;
}

bool AudioRecorder::getSoundCardInfo(std::string &soundCardInfo, int soundCardIndex)
{
    if (soundCardIndex < 0)
    {
        return false;
    }

    soundCardInfo.clear();

    // Open sound card handler
    snd_ctl_t *sndCardHandler = NULL;
    {
        std::stringstream ss;
        ss << "hw:" << soundCardIndex;

        if (snd_ctl_open(&sndCardHandler, ss.str().c_str(), 0) != 0)
            return false;

        ss.str("");
        ss << "Card " << soundCardIndex << ": ";

        soundCardInfo += ss.str();
    }

    // Get sound card info
    {
        snd_ctl_card_info_t *sndCardInfo = NULL;

        if (snd_ctl_card_info_malloc(&sndCardInfo) != 0)
        {
            errStr = "Allocation memory for sound card info error!";
            ERR(errStr);
            snd_ctl_close(sndCardHandler);
            return false;
        }

        if (snd_ctl_card_info(sndCardHandler, sndCardInfo) != 0)
        {
            errStr = "Getting sound card info error!";
            ERR(errStr);
            snd_ctl_close(sndCardHandler);
            return false;
        }

        soundCardInfo += snd_ctl_card_info_get_name(sndCardInfo);
        soundCardInfo += ":\n";

//        DBG("Card = " << snd_ctl_card_info_get_card(sndCardInfo));
//        DBG("Id = \"" << snd_ctl_card_info_get_id(sndCardInfo) << '\"');
//        DBG("Driver = \"" << snd_ctl_card_info_get_card(sndCardInfo) << '\"');
//        DBG("Name = \"" << snd_ctl_card_info_get_id(sndCardInfo) << '\"');
//        DBG("Long name = \"" << snd_ctl_card_info_get_longname(sndCardInfo) << '\"');
//        DBG("Mixer name = \"" << snd_ctl_card_info_get_mixername(sndCardInfo) << '\"');
//        DBG("Components = \"" << snd_ctl_card_info_get_components(sndCardInfo) << '\"');

        snd_ctl_card_info_free(sndCardInfo);
    }

    // Get devices info
    int devIndex = -1;
    for (;;)
    {
        // Get next device
        if (snd_ctl_pcm_next_device(sndCardHandler, &devIndex) != 0 || devIndex == -1)
            break;

        std::string deviceId;
        {
            std::stringstream ss;
            ss << "plughw:" << soundCardIndex << ',' << devIndex;
            deviceId = ss.str();
        }

        // Get device info
        std::string deviceName;
        if (!getDeviceName(deviceName, sndCardHandler, devIndex, true))
            continue;

        soundCardInfo += '\t' + deviceName + ": " + deviceId + " - playback\n";

        if (!getDeviceName(deviceName, sndCardHandler, devIndex, false))
            continue;

        soundCardInfo += '\t' + deviceName + ": " + deviceId + " - capture\n";
    }

    snd_ctl_close(sndCardHandler);

    soundCardInfo.erase(soundCardInfo.size() - 1, 1);

    return true;
}

std::string AudioRecorder::getLastErrorInfo()
{
    return errStr;
}

void AudioRecorder::init(int argc, char **argv)
{
    static const struct option cmdLineOptions[] =
    {
        {"capture_dev",  required_argument, NULL, 'C'},
        {"chans_number", optional_argument, NULL, 'c'},
        {"gain",         required_argument, NULL, 'g'},
        {"help",         no_argument,       NULL, 'h'},
        {"list",         no_argument,       NULL, 'l'},
        {"out_file",     required_argument, NULL, 'o'},
        {"sample_rate",  required_argument, NULL, 's'},
        {"time_to_rec",  required_argument, NULL, 't'},
        {"verbose",      no_argument,       NULL, 'v'},
        {0, 0, 0, 0}
    };

    inited = false;

    if (argc < 2)
    {
        PRINT(helpStr);
        return;
    }

    for (int res = 0; res != -1; )
    {
        int optionIndex = 0;
        res = getopt_long(argc, argv, "C:c:g:hlo:s:t:v", cmdLineOptions, &optionIndex);

        if (res == '?')
            continue;

        if (res == 'C')
        {
            captureDevIdStr = optarg;
//            DBG("captureDevIdStr = \"" << captureDevIdStr << '\"');
        }
        else if (res == 'c')
        {
            if (optarg)
            {
                stringToInt(optarg, &chansNumber);
//                DBG("chansNumber = " << chansNumber);

                if (chansNumber < 1 || chansNumber > 2)
                {
                    errStr = "Missing value for channels number. Must be 1 or 2!";
                    ERR(errStr);
                    return;
                }
            }
        }
        else if (res == 'g')
        {
            std::stringstream ss;
            ss << optarg;
            ss >> gainFactor;

//            DBG("gainValue = " << gainValue);

            if (gainFactor < -40.0 || gainFactor > 40.0)
            {
                errStr = "Missing value for gain factor! Must be >= -40.0 and <= 40.0!";
                ERR(errStr);
                return;
            }
        }
        else if (res == 'h')
        {
            PRINT(helpStr);
            return;
        }
        else if (res == 'l')
        {
            std::vector<std::string> hwInfo = getAudioDevsList();
            for (std::vector<std::string>::iterator it = hwInfo.begin(); it != hwInfo.end(); ++it)
                PRINT(*it);

            return;
        }
        else if (res == 'o')
        {
            outFileStr = optarg;
//            DBG("outFileStr = \"" << outFileStr << '\"');
        }
        else if (res == 's')
        {
            stringToInt(optarg, &sampleRate);
//            DBG("sampleRate = " << sampleRate);
        }
        else if (res == 't')
        {
            stringToInt(optarg, &timeToRec);
//            DBG("timeToRec = " << timeToRec);
        }
        else if (res == 'v')
            verbose = true;
    }

    if (!validateParams())
        return;

//    HERE();
    inited = createAudioBuf();
}

void AudioRecorder::stringToInt(char *str, unsigned int *pIntValue)
{
    std::stringstream ss;
    ss << str;
    ss >> *pIntValue;
}

bool AudioRecorder::validateParams()
{
    if (captureDevIdStr.empty())
    {
        errStr = "Capture device ID not specified!\nUse: -C,--capture_dev <ID string>";
        ERR(errStr);
        return false;
    }

    if (outFileStr.empty())
    {
        errStr = "Output file name not specified!\nUse: -o,--out_file <path>";
        ERR(errStr);
        return false;
    }

    if (sampleRate == 0)
    {
        errStr = "Sample rate not specified!\nUse: -s,--sample_rate <value>";
        ERR(errStr);
        return false;
    }

    if (timeToRec == 0)
    {
        errStr = "Recording duration not specified!\nUse: -t,--time_to_rec <duration in seconds>";
        ERR(errStr);
        return false;
    }

    return true;
}

void AudioRecorder::wavHeaderInit()
{
    strncpy(wavHeader.fields.chunkId, "RIFF", 4);
    strncpy(wavHeader.fields.format, "WAVE", 4);
    strncpy(wavHeader.fields.subchunk1Id, "fmt ", 4);
    wavHeader.fields.subchunk1Size = 16;
    wavHeader.fields.audioFormat = 1;
    wavHeader.fields.bitsPerSample = 16;
    strncpy(wavHeader.fields.subchunk2Id, "data", 4);
}
