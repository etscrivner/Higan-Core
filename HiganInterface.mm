/*
 Copyright (c) 2013, OpenEmu Team


 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the OpenEmu Team nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY OpenEmu Team ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL OpenEmu Team BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "HiganInterface.h"

#include <nall/stream.hpp>

Interface::Interface()
{
    videoBuffer = new uint32_t[512 * 480];
    
    emulator(OESuperFamicomSystem)   = new SuperFamicom::Interface;
    emulator(OEGameBoySystem)        = new GameBoy::Interface;
    emulator(OEGameBoyAdvanceSystem) = new GameBoyAdvance::Interface;
    emulator(OEFamicomSystem)        = new Famicom::Interface;

    for(auto& system : emulator) system->bind = this;
}

Interface::~Interface()
{
    delete [] videoBuffer;
}

void Interface::loadRequest(unsigned id, string name, string type, bool required)
{
    paths(id) = gamePaths(0);
    active->load(id);
}

void Interface::loadRequest(unsigned id, string path, bool required)
{
    NSLog(@"Loading file \"%s\"", path.data());

    string fullpath = {paths(active->group(id)), path};
    if(file::exists(fullpath) == true)
    {
        mmapstream stream(fullpath);
        return active->load(id, stream);
    }

    NSLog(@"Higan: Wasn't able to load file \"%s\"", path.data());
}

void Interface::saveRequest(unsigned id, string path)
{
    NSLog(@"Saving file \"%s\"", path.data());
    
    string pathname = {paths(active->group(id)), path};
    filestream stream(pathname, file::mode::write);
    active->save(id, stream);
}

uint32_t Interface::videoColor(unsigned source, uint16_t a, uint16_t r, uint16_t g, uint16_t b)
{
    a >>= 8, r >>= 8, g >>= 8, b >>= 8;
    return a << 24 | r << 16 | g << 8 | b << 0;
}

void Interface::videoRefresh(const uint32_t* palette, const uint32_t* data, unsigned pitch, unsigned w, unsigned h)
{
    unsigned outputPitch = 512;
    pitch >>= 2;

    width = w;
    height = h;

    if(activeSystem == OESuperFamicomSystem)
    {
        // Remove overscan
        data += 8 * pitch;
        if(height == 240)
        {
            height = 224;
        }
        else if(height == 480)
        {
            height = 448;
        }
    }

    for(unsigned y = 0; y < height; y++)
    {
        const uint32_t* sp = data + y * pitch;
        uint32_t* dp = videoBuffer + y * outputPitch;
        for(unsigned x = 0; x < width; x++)
        {
            *dp++ = palette[*sp++];
        }
    }
}

void Interface::audioSample(int16_t lsample, int16_t rsample)
{
    signed samples[] = {lsample, rsample};
    resampler.sample(samples);
}

int16_t Interface::inputPoll(unsigned port, unsigned device, unsigned input)
{
    return inputState[port][input];
}

unsigned Interface::dipSettings(const Markup::Node& node)
{
    NSLog(@"dipSettings(const Markup::Node& node) not implemented");
}

string Interface::path(unsigned group)
{
    return paths(group);
}

string Interface::server()
{
    return "";
}

void Interface::notify(string text)
{
    NSLog(@"Higan: %s", text.data());
}

void Interface::loadMedia(string path, string systemName, OESystemIndex emulatorIndex, unsigned mediaID)
{    
    paths(0) = {bundlePath, "/", systemName, ".sys/"};
    paths(mediaID) = {supportPath, "/", systemName, "/", path, "/"};
    directory::create(paths(mediaID));
    gamePaths.append(paths(mediaID));

    this->mediaID = mediaID;
    
    activeSystem = emulatorIndex;
    active = emulator(emulatorIndex);
}

void Interface::load()
{
    active->load(mediaID);

    active->paletteUpdate(Emulator::Interface::PaletteMode::Emulation);
    initializeResampler();

    active->power();

    run();
}

void Interface::run()
{
    active->run();
}

void Interface::initializeResampler()
{
    resampler.setFrequency(active->audioFrequency());
}

