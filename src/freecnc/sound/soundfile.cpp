#include <cassert>
#include <limits>
#include <memory>

#include "SDL.h"

#include "soundfile.h"
#include "../lib/fcncendian.h"

namespace Sound
{
    const int steps[89] = {
        7,     8,     9,     10,    11,    12,     13,    14,    16,
        17,    19,    21,    23,    25,    28,     31,    34,    37,
        41,    45,    50,    55,    60,    66,     73,    80,    88,
        97,    107,   118,   130,   143,   157,    173,   190,   209,
        230,   253,   279,   307,   337,   371,    408,   449,   494,
        544,   598,   658,   724,   796,   876,    963,   1060,  1166,
        1282,  1411,  1552,  1707,  1878,  2066,   2272,  2499,  2749,
        3024,  3327,  3660,  4026,  4428,  4871,   5358,  5894,  6484,
        7132,  7845,  8630,  9493,  10442, 11487,  12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086,  29794, 32767
    };
    const int index_adjustment[8] = {-1,-1,-1,-1,2,4,6,8};

    // Decode Westwood's ADPCM format.  Original code from ws-aud.txt by Asatur V. Nazarian
    #define HIBYTE(word) ((word) >> 8)
    #define LOBYTE(word) ((word) & 0xFF)

    const int WSTable2bit[4] = {-2,-1,0,1};
    const int WSTable4bit[16] = {
        -9, -8, -6, -5, -4, -3, -2, -1,
         0,  1,  2,  3,  4,  5,  6,  8
    };

    template<class OutputType, class InputType>
    OutputType Clip(InputType value, InputType min, InputType max)
    {
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return static_cast<OutputType>(value);
    }

    template<class OutputType, class InputType>
    OutputType Clip(InputType value)
    {
        return Clip<OutputType, InputType>(value, std::numeric_limits<OutputType>::min(), std::numeric_limits<OutputType>::max());
    }

    void IMADecode(unsigned char *output, SampleIterator input, unsigned short compressed_size, int& sample, int& index)
    {
        if (compressed_size==0)
            return;

        unsigned short uncompressed_size = compressed_size * 2;

        short* formatted_output = reinterpret_cast<short*>(output);

        // Each byte is processed twice: lower then upper half
        for (int samples = 0; samples < uncompressed_size; ++samples) {
            unsigned char code;
            if (samples & 1) {
                code = (*input++) >> 4;
            } else {
                code = (*input) & 0xF;
            }

            int step = steps[index];
            int delta = step >> 3;

            if (code & 1) {
                delta += step >> 2;
            }
            if (code & 2) {
                delta += step >> 1;
            }
            if (code & 4) {
                delta += step;
            }
            if (code & 8) {
                sample -= delta;
            } else {
                sample += delta;
            }

            sample = Clip<short>(sample);
            *formatted_output++ = sample;

            index += index_adjustment[code & 7];
            index = Clip<unsigned char>(index, 0, 88);
        }
    }

    // Decode Westwood's ADPCM format.  Original code from ws-aud.txt by Asatur V. Nazarian

    void WSADPCM_Decode(unsigned char *output, SampleIterator input, unsigned short compressed_size, unsigned short uncompressed_size)
    {
        short CurSample;
        unsigned char  code;
        char  count;
        unsigned short i;
        unsigned short shifted_input;

        if (compressed_size==uncompressed_size) {
            std::copy(input, input+uncompressed_size, output);
            return;
        }

        CurSample=0x80;
        i=0;

        unsigned short bytes_left = uncompressed_size;
        while (bytes_left>0) { // expecting more output
            shifted_input=input[i++];
            shifted_input<<=2;
            code=HIBYTE(shifted_input);
            count=LOBYTE(shifted_input)>>2;
            switch (code) // parse code
            {
            case 2: // no compression...
                if (count & 0x20) {
                    count<<=3;  // here it's significant that (count) is signed:
                    CurSample+=count>>3; // the sign bit will be copied by these shifts!
                    *output++ = Clip<unsigned char>(CurSample);
                    bytes_left--; // one byte added to output
                } else {
                    // copy (count+1) bytes from input to output

                    /// @TODO This version doesn't produce the same result as the loop below
                    //std::copy(input+i, input+i+count+1, output);
                    //bytes_left -= (count + 1);
                    //i += (count + 1);

                    // Old version for reference:
                    for (++count; count > 0; --count) {
                        --bytes_left;
                        *output++ = input[i++];
                    }

                    CurSample=input[i-1]; // set (CurSample) to the last byte sent to output
                }
                break;
            case 1: // ADPCM 8-bit -> 4-bit
                for (count++;count>0;count--) { // decode (count+1) bytes
                    code=input[i++];
                    CurSample+=WSTable4bit[(code & 0x0F)]; // lower nibble
                    *output++ =  Clip<unsigned char>(CurSample);
                    CurSample+=WSTable4bit[(code >> 4)]; // higher nibble
                    *output++ =  Clip<unsigned char>(CurSample);
                    bytes_left-=2; // two bytes added to output
                }
                break;
            case 0: // ADPCM 8-bit -> 2-bit
                for (count++;count>0;count--) { // decode (count+1) bytes
                    code=input[i++];
                    CurSample+=WSTable2bit[(code & 0x03)]; // lower 2 bits
                    *output++ =  Clip<unsigned char>(CurSample);
                    CurSample+=WSTable2bit[((code>>2) & 0x03)]; // lower middle 2 bits
                    *output++ =  Clip<unsigned char>(CurSample);
                    CurSample+=WSTable2bit[((code>>4) & 0x03)]; // higher middle 2 bits
                    *output++ =  Clip<unsigned char>(CurSample);
                    CurSample+=WSTable2bit[((code>>6) & 0x03)]; // higher 2 bits
                    *output++ =  Clip<unsigned char>(CurSample);
                    bytes_left-=4; // 4 bytes sent to output
                }
                break;
            default: // just copy (CurSample) (count+1) times to output
                /// @TODO This version doesn't produce the same result as the loop below
                //std::fill(output, output+count+2, Clip<unsigned char>(CurSample));
                //bytes_left -= count+1;

                // Old version for reference:
                for (count++;count>0;count--,bytes_left--)
                    *output++ = Clip<unsigned char>(CurSample);
            }
        }
    }
}

namespace
{
    SDL_AudioCVT monoconv;
    SDL_AudioCVT eightbitconv;
    bool initconv = false;

    SampleBuffer chunk(SOUND_MAX_CHUNK_SIZE);
    unsigned char tmpbuff[SOUND_MAX_UNCOMPRESSED_SIZE * 4];
}

SoundFile::SoundFile() : fileOpened(false)
{
    if (!initconv) {
        if (SDL_BuildAudioCVT(&eightbitconv, AUDIO_U8, 1, 22050, SOUND_FORMAT, SOUND_CHANNELS, SOUND_FREQUENCY) < 0) {
            game.log << "Could not build 8bit->16bit conversion filter" << endl;
            return;
        }

        if (SDL_BuildAudioCVT(&monoconv, AUDIO_S16SYS, 1, 22050, SOUND_FORMAT, SOUND_CHANNELS, SOUND_FREQUENCY) < 0) {
            game.log << "Could not build mono->stereo conversion filter" << endl;
            return;
        }
        initconv = true;
    }   
}

SoundFile::~SoundFile()
{
    Close();
}

bool SoundFile::Open(const std::string& filename)
{
    Close();
    
    // Open file
    file = game.vfs.open(filename);
    if (!file) {
        game.log << "Sound: Could not open file \"" << filename << "\"." << endl;
        return false;
    }
    
    if (file->size() < 12) {
        game.log << "Sound: Could not open file \"" << filename << "\": Invalid file size." << endl;
    }

    // Parse header
    vector<unsigned char> data(12);
    file->read(data, 12);
    vector<unsigned char>::iterator it(data.begin());

    frequency   = read_word(it, FCNC_LIL_ENDIAN);
    comp_size   = read_dword(it, FCNC_LIL_ENDIAN);
    uncomp_size = read_dword(it, FCNC_LIL_ENDIAN);
    flags       = read_byte(it);
    type        = read_byte(it);

    // Check for known format
    if (type == 1) {
        conv = &eightbitconv;
    } else if (type == 99) {
        conv = &monoconv;
    } else {
        game.log << "Sound: Could not open file \"" << filename << "\": Corrupt header (Unknown type: " << type << ")." << endl;
        return false;
    }

    if (frequency != 22050)
        game.log << "Sound: \"" << filename << "\" needs converting from " << frequency << "Hz (should be 22050Hz)" << endl;

    imaSample = 0;
    imaIndex  = 0;
    
    this->filename = filename;
    fileOpened = true;

    return true;
}

void SoundFile::Close()
{
    if (fileOpened) {
        file.reset();
        fileOpened = false;
    }
}

unsigned int SoundFile::Decode(SampleBuffer& buffer, unsigned int length)
{
    if (!initconv || !fileOpened)
        return SOUND_DECODE_ERROR;

    assert(buffer.empty());

    unsigned int max_size = length == 0 ? uncomp_size * conv->len_mult : length;
    buffer.resize(max_size);

    unsigned short comp_sample_size, uncomp_sample_size;
    unsigned int ID;

    vector<unsigned char> header;
    vector<unsigned char>::iterator it;
    unsigned int written = 0;
    while ((file->pos()+8) < file->size()) {
        // Each sample has a header
        file->read(header, 8);
        it = header.begin();
        comp_sample_size    = read_word(it, FCNC_LIL_ENDIAN);
        uncomp_sample_size  = read_word(it, FCNC_LIL_ENDIAN);
        ID                  = read_dword(it, FCNC_LIL_ENDIAN);

        if (comp_sample_size > (SOUND_MAX_CHUNK_SIZE)) {
            game.log << "Size data for current sample too large" << endl;
            return SOUND_DECODE_ERROR;
        }

        // abort if id was wrong */
        if (ID != 0xDEAF) {
            game.log << "Sample had wrong ID: Got " << std::hex << ID << std::dec << " expected 0xDEAF." << endl;
            return SOUND_DECODE_ERROR;
        }

        if (written + uncomp_sample_size*conv->len_mult > max_size) {
            file->seek_cur(-8); // rewind stream back to headers
            buffer.resize(written); // Truncate buffer.
            return SOUND_DECODE_STREAMING;
        }

        // compressed data follows header
        file->read(chunk, comp_sample_size);
        SampleIterator chit = chunk.begin();
        if (type == 1) {
            Sound::WSADPCM_Decode(tmpbuff, chit, comp_sample_size, uncomp_sample_size);
        } else {
            Sound::IMADecode(tmpbuff, chit, comp_sample_size, imaSample, imaIndex);
        }
        conv->buf = tmpbuff;
        conv->len = uncomp_sample_size;
        if (SDL_ConvertAudio(conv) < 0) {
            game.log << "Could not run conversion filter: " << SDL_GetError() << endl;
            return SOUND_DECODE_ERROR;
        }
        memcpy(&buffer[written], tmpbuff, uncomp_sample_size*conv->len_mult);
        //offset += 8 + comp_sample_size;
        written += uncomp_sample_size*conv->len_mult;
    }

    // Truncate if final sample was too small.
    if (written < max_size)
        buffer.resize(written);

    return SOUND_DECODE_COMPLETED;    
}
