/*
 * AudioFileSourceLoop
 * Read SD/LittleFS file to be used by AudioGenerator
 * Reads file in a loop (for looped playback)
 * 
 * Thomas Winischhofer (A10001986), 2023
 *
 * Based on AudioFileSourceSD by Earle F. Philhower, III
 *
 */

#include "remote_global.h"
#include "AudioFileSourceLoop.h"

AudioFileSourceLoop::~AudioFileSourceLoop()
{
    if(f) f.close();
}

uint32_t AudioFileSourceLoop::read(void *data, uint32_t len)
{
    uint32_t glen = 0, g, p, l = 5;

    while(l--) {
        if(endPos) {
            if((p = f.position()) < endPos) {
                g = f.read((uint8_t *)data, (p + len > endPos) ? endPos - p : len);
            } else g = 0;
        } else {
            g = f.read((uint8_t *)data, len);
        }
        glen += g;
        len  -= g;
        if(!doPlayLoop || !len) return glen;
        data = (void *)((uint8_t *)data + g);
        seek(startPos, SEEK_SET);
    }

    return glen;

    /*
    uint32_t glen = f.read(reinterpret_cast<uint8_t*>(data), len);
    if(!doPlayLoop || glen == len) return glen;
    seek(startPos, SEEK_SET);
    return glen + f.read(reinterpret_cast<uint8_t*>(data) + glen, len - glen);
    */
}

bool AudioFileSourceLoop::seek(int32_t pos, int dir)
{
    if(!f) return false;
    if(dir == SEEK_SET)      return f.seek(pos);
    else if(dir == SEEK_CUR) return f.seek(f.position() + pos);
    else if(dir == SEEK_END) return f.seek(f.size() + pos);
    return false;
}

// SD -----------------------------------------------

AudioFileSourceSDLoop::AudioFileSourceSDLoop()
{
}

/*
AudioFileSourceSDLoop::AudioFileSourceSDLoop(const char *filename)
{
    open(filename);
}
*/

bool AudioFileSourceSDLoop::open(const char *filename)
{
    f = SD.open(filename, FILE_READ);
    return f;
}

// FlashFS -------------------------------------------

AudioFileSourceFSLoop::AudioFileSourceFSLoop()
{
}

/*
AudioFileSourceFSLoop::AudioFileSourceFSLoop(const char *filename)
{
    open(filename);
}
*/

bool AudioFileSourceFSLoop::open(const char *filename)
{
    f = LittleFS.open(filename, FILE_READ);
    return f;
}
