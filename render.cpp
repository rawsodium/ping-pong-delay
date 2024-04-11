/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

http://bela.io

C++ Real-Time Audio Programming with Bela - Lecture 11: Circular buffers
circular-buffer: template code for implementing delays
*/

#include <Bela.h>
#include <vector>
#include "MonoFilePlayer.h"

// Name of the sound file (in project folder)
std::string gFilename = "slow-drum-loop.wav";

// Object that handles playing sound from a buffer
MonoFilePlayer gPlayer;

// declare variables for circular buffers
std::vector<float> delayBufferLeftChannel;
std::vector<float> delayBufferRightChannel;

unsigned int writePointerLeft = 0;
unsigned int writePointerRight = 0;
unsigned int readPointerLeft = 0;
unsigned int readPointerRight = 0;
//unsigned int gOffset = 0;

bool setup(BelaContext *context, void *userData)
{
    // Load the audio file
    if(!gPlayer.setup(gFilename)) {
        rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
        return false;
    }

    // Print some useful info
    rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds)\n",
              gFilename.c_str(), gPlayer.size(),
              gPlayer.size() / context->audioSampleRate);

    // allocate the circular buffers to contain enough samples to cover 0.5 seconds
    delayBufferLeftChannel.resize(0.5 * context->audioSampleRate);
    delayBufferRightChannel.resize(0.5 * context->audioSampleRate);

    // set initial delay to 0.1 seconds on each side, for now.
    readPointerLeft = (writePointerLeft - (int)(0.1 * context->audioSampleRate) + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
    readPointerRight = (writePointerRight - (int)(0.1 * context->audioSampleRate) + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

    return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // read delay times from trimmers (L in input1, R in input2)
        float delayLeft = analogRead(context, n/2, 0);  // read in analog 0
        float delayRight = analogRead(context, n/2, 1); // read in analog 1
        int delayInSampsLeft = delayLeft * context->audioSampleRate;
        int delayInSampsRight = delayRight * context->audioSampleRate;

        // use modulo to browse circular buffers
        readPointerLeft = (writePointerLeft - delayInSampsLeft + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
        readPointerRight = (writePointerRight - delayInSampsRight + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

        // read feedback from trimmer
        float feedback = analogRead(context, n/2, 2); // read in analog 2

        // process input sample
        float in = gPlayer.process();

        // Read the output from the buffer, at the location expressed by the offset
        float outLeft = delayBufferLeftChannel[readPointerLeft];
        float outRight = delayBufferRightChannel[readPointerRight];

        // write input and feedback to buffers
        delayBufferLeftChannel[writePointerLeft] = in + feedback * outLeft;
        delayBufferRightChannel[writePointerRight] = in + feedback * outRight;

        // increment pointers
        writePointerLeft++;
        writePointerRight++;
        readPointerLeft++;
        readPointerRight++;

        // wrap pointers if need be
        if (writePointerLeft >= delayBufferLeftChannel.size()) {
            writePointerLeft = 0;
        } else if (writePointerRight >= delayBufferRightChannel.size()) {
            writePointerRight = 0;
        } else if (readPointerLeft >= delayBufferLeftChannel.size()) {
            readPointerLeft = 0;
        } else if (readPointerRight >= delayBufferRightChannel.size()) {
            readPointerRight = 0;
        }

        // Write the L and R different channels
        audioWrite(context, n, 0, outLeft);
        audioWrite(context, n, 1, outRight);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}