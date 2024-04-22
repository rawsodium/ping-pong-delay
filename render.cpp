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
#include <cmath>
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

// keep track of prev,second, and third previous delay trimmer vals
float prevDelayL = 0.0;
float prevDelayR = 0.0;
float secondPrevDelayL = 0.0;
float secondPrevDelayR = 0.0;
float thirdPrevDelayL = 0.0;
float thirdPrevDelayR = 0.0;

// keep track of prev delay in samp vals
float prevDelayInSampsL = 0.0;
float prevDelayInSampsR = 0.0;

// Filter state and coefficients
float gA1 = 0.0, gA2 = 0.0;
float gB0 = 1.0, gB1 = 0.0, gB2 = 0.0;

float gPrevInLeft = 0.0;
float gPrev2InLeft = 0.0;
float gPrevInRight = 0.0;
float gPrev2InRight = 0.0;
float gPrevOutLeft = 0.0;
float gPrev2OutLeft = 0.0;
float gPrevOutRight = 0.0;
float gPrev2OutRight = 0.0;

// calculate coefficients for LPF
void calculate_coefficients(float sampleRate, float frequency, float q)
{
    float k = tanf(M_PI * frequency / sampleRate);
    float norm = 1.0 / (1 + k / q + k * k);

    gB0 = k * k * norm;
    gB1 = 2.0 * gB0;
    gB2 = gB0;
    gA1 = 2 * (k * k - 1) * norm;
    gA2 = (1 - k / q + k * k) * norm;
}


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
    delayBufferLeftChannel.resize(0.75 * context->audioSampleRate);
    delayBufferRightChannel.resize(0.75 * context->audioSampleRate);

    // set initial delay to 0.1 seconds on each side, for now.
    readPointerLeft = (writePointerLeft - (int)(0.1 * context->audioSampleRate) + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
    readPointerRight = (writePointerRight - (int)(0.1 * context->audioSampleRate) + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

    // Calculate initial coefficients for lowpass filter
    calculate_coefficients(context->audioSampleRate, 10000, 0.707);
    return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // read delay times from trimmers (L in input1, R in input2)
        float input0 = analogRead(context, n/2, 0); // read in analog 0
        float input1 = analogRead(context, n/2, 1); // read in analog 1
        // read feedback from trimmer
        float input2 = analogRead(context, n/2, 2); // read in analog 2
        // read wet/dry mix from trimmer
        float input3 = analogRead(context, n/2, 3); // read in analog 3

        // map delay times to normal ranges - 1ms to 500ms
        float delayLeft = map(input0, 0, 3.3 / 4.096, 0.01, 0.5);
        float delayRight = map(input1, 0, 3.3 / 4.096, 0.01, 0.5);

        // average the current and last three delay values read from the trimmers, to reduce distortion
        float averagedDelayLeft = (delayLeft + prevDelayL + secondPrevDelayL + thirdPrevDelayL) / 4.0;
        float averagedDelayRight = (delayRight + prevDelayR + secondPrevDelayR + thirdPrevDelayR) / 4.0;

        thirdPrevDelayL = secondPrevDelayL;
        secondPrevDelayL = prevDelayL;
        prevDelayL = delayLeft;
        thirdPrevDelayR = secondPrevDelayR;
        secondPrevDelayR = prevDelayR;
        prevDelayR = delayRight;

        // calculate delay in samples for left and right channels
        int delayInSampsLeft = averagedDelayLeft * context->audioSampleRate;
        int delayInSampsRight = averagedDelayRight * context->audioSampleRate;

        // average current delay in samples and previous delay in samples to reduce distortion
        int avgDISL = (delayInSampsLeft + prevDelayInSampsL) / 2;
        int avgDISR = (delayInSampsRight + prevDelayInSampsR) / 2;

        prevDelayInSampsL = delayInSampsLeft;
        prevDelayInSampsR = delayInSampsRight;

        // use modulo to browse circular buffers
        readPointerLeft = (writePointerLeft - avgDISL + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
        readPointerRight = (writePointerRight - avgDISR + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

        // map feedback and mix values to normal ranges (0 to 1)
        float feedback = map(input2, 0, 3.3 / 4.096, 0, 0.75);
        float wetFactor = map(input3, 0, 3.3 / 4.096, 0, 1.0);

        float dryFactor = 1.0f - wetFactor;

        // process input sample
        float in = gPlayer.process();

        // read at fractional place in delay buffers
        int indexBelowLeft = floorf(readPointerLeft);
        int indexAboveLeft = indexBelowLeft + 1;
        if(indexAboveLeft >= delayBufferLeftChannel.size())
            indexAboveLeft = 0;
        float fractionAboveLeft = readPointerLeft - indexBelowLeft;
        float fractionBelowLeft = 1.0 - fractionAboveLeft;

        float delayBufLeft = fractionBelowLeft * delayBufferLeftChannel[indexBelowLeft] + fractionAboveLeft * delayBufferLeftChannel[indexAboveLeft];

        int indexBelowRight = floorf(readPointerRight);
        int indexAboveRight = indexBelowRight + 1;
        if(indexAboveRight >= delayBufferRightChannel.size())
            indexAboveRight = 0;
        float fractionAboveRight = readPointerRight - indexBelowRight;
        float fractionBelowRight = 1.0 - fractionAboveRight;

        float delayBufRight = fractionBelowRight * delayBufferRightChannel[indexBelowRight] + fractionAboveRight * delayBufferRightChannel[indexAboveRight];

        // feed left channel into right and vice versa
        float outLeft = in * dryFactor + delayBufRight * wetFactor;
        float outRight = in * dryFactor + delayBufLeft * wetFactor;

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

        // LPF on left and right outputs
        outLeft = ((gB0 * outLeft) + (gB1 * gPrevInLeft) + (gB2 * gPrev2InLeft)) - ((gA1 * gPrevOutLeft) + (gA2 * gPrev2OutLeft));
        gPrev2OutLeft = gPrevOutLeft;
        gPrevOutLeft = outLeft;

        gPrev2InLeft = gPrevInLeft;
        gPrevInLeft = outLeft;

        outRight = ((gB0 * outRight) + (gB1 * gPrevInRight) + (gB2 * gPrev2InRight)) - ((gA1 * gPrevOutRight) + (gA2 * gPrev2OutRight));
        gPrev2OutRight = gPrevOutRight;
        gPrevOutRight = outRight;

        gPrev2InRight = gPrevInRight;
        gPrevInRight = outRight;

        // attenuate output for right now while we cut distortion
        outLeft *= 0.5;
        outRight *= 0.5;

        // Write the L and R different channels
        audioWrite(context, n, 0, outLeft);
        audioWrite(context, n, 1, outRight);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}