/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

http://bela.io

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

// calculates output with fractional read pointer.
float processFractionalOut(std::vector<float> buf, int readPtr) {
    int indexBelow = floorf(readPtr);
    int indexAbove = indexBelow + 1;
    if(indexAbove >= buf.size())
        indexAbove = 0;

    float fractionAbove = readPtr - indexBelow;
    float fractionBelow = 1.0 - fractionAbove;

    return fractionBelow * buf[indexBelow] + fractionAbove * buf[indexAbove];
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // TODO: use fractional read ptr somewhere... tbd

        // read delay times from trimmers (L in input1, R in input2)
        float input0 = analogRead(context, n/2, 0);  // read in analog 0
        float input1 = analogRead(context, n/2, 1); // read in analog 1

        // map delay times to normal ranges - 1ms to 1000ms
        float delayLeft = map(input0, 0, 3.3 / 4.096, 0.01, 1);
        float delayRight = map(input1, 0, 3.3 / 4.096, 0.01, 1);

        int delayInSampsLeft = delayLeft * context->audioSampleRate;
        int delayInSampsRight = delayRight * context->audioSampleRate;

        // use modulo to browse circular buffers
        readPointerLeft = (writePointerLeft - delayInSampsLeft + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
        readPointerRight = (writePointerRight - delayInSampsRight + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

        // read feedback from trimmer
        float input2 = analogRead(context, n/2, 2); // read in analog 2

        // read wet/dry mix from trimmer
        float input3 = analogRead(context, n/2, 3); // read in analog 3

        // map feedback and mix values to normal ranges (0 to 1)
        float feedback = map(input2, 0, 3.3 / 4.096, 0, 0.99);
        float wetFactor = map(input3, 0, 3.3 / 4.096, 0, 1);

        float dryFactor = 1.0f - wetFactor;

        // process input sample
        float in = gPlayer.process();

        // read at fractional place in delay buffers
        /*
         * this does not work... if you substitute delayBufferLeftChannel[readPointerLeft]
         * and delayBufferRightChannel[readPointerRight] for their respective variables here, you will cause
         * some nasty high-pitched audio artifacts that will require you to restart Bela. I do not recommend
         */
        //float delayBufLeft = processFractionalOut(delayBufferLeftChannel, readPointerLeft);
        //float delayBufRight = processFractionalOut(delayBufferRightChannel, readPointerRight);

        // Read the output from the buffer
        float outLeft = in * dryFactor + delayBufferLeftChannel[readPointerLeft] * wetFactor;
        float outRight = in * dryFactor + delayBufferRightChannel[readPointerRight] * wetFactor;

        // this is literally to make the crunchiness less harsh, you should comment these out
        outLeft *= 0.25;
        outRight *= 0.25;

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