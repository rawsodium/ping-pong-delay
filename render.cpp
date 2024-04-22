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
#include <libraries/Biquad/Biquad.h>
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

float delayLeft = 0.0;
float delayRight = 0.0;
float feedback = 0.0;
float wetFactor = 0.0;
float volume = 1.0;

int indexAboveLeft = 0;
int indexAboveRight = 0;
int indexBelowLeft = 0;
int indexBelowRight = 0;
float fractionAboveLeft = 0.0;
float fractionAboveRight = 0.0;
float fractionBelowLeft = 0.0;
float fractionBelowRight = 0.0;

float in = 0.0;

// store current and last button states
int currentButtonStatus = HIGH;
int lastButtonStatus = HIGH;

// button pin
const int kButtonPin = 6;

// use lp filter on L and R channels to cut noise... hopefully
Biquad lpLeft;
Biquad lpRight;


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

    // settings for filters
    Biquad::Settings settings{ .fs = context->audioSampleRate, .type = Biquad::lowpass};

    // setup filter with proper parameters
    lpLeft.setup(settings);
    lpLeft.setQ(0.71);
    lpLeft.setPeakGain(1.0);

    lpRight.setup(settings);
    lpRight.setQ(0.71);
    lpRight.setPeakGain(1.0);

    // set initial delay to 0.1 seconds on each side, for now.
    readPointerLeft = (writePointerLeft - (int)(0.1 * context->audioSampleRate) + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
    readPointerRight = (writePointerRight - (int)(0.1 * context->audioSampleRate) + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

    return true;
}

void render(BelaContext *context, void *userData)
{
    // set center frequency of each notch filter
    lpLeft.setFc(820);
    lpRight.setFc(820);

    // average the current and last three delay values read from the trimmers, to reduce jittering
    float averagedDelayLeft = (delayLeft + prevDelayL + secondPrevDelayL + thirdPrevDelayL) / 4.0;
    float averagedDelayRight = (delayRight + prevDelayR + secondPrevDelayR + thirdPrevDelayR) / 4.0;

    int delayInSampsLeft = averagedDelayLeft * context->audioSampleRate;
    int delayInSampsRight = averagedDelayRight * context->audioSampleRate;

    int avgDISL = (delayInSampsLeft + prevDelayInSampsL) / 2;
    int avgDISR = (delayInSampsRight + prevDelayInSampsR) / 2;

    // use modulo to browse circular buffers
    readPointerLeft = (writePointerLeft - avgDISL  + delayBufferLeftChannel.size()) % delayBufferLeftChannel.size();
    readPointerRight = (writePointerRight - avgDISR  + delayBufferRightChannel.size()) % delayBufferRightChannel.size();

    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // check for a button press
        currentButtonStatus = digitalRead(context, n, kButtonPin);

        // read delay times from trimmers (L in input0, R in input1)
        float input0 = analogRead(context, n/2, 0); // read in analog 0
        float input1 = analogRead(context, n/2, 1); // read in analog 1
        // read feedback from trimmer
        float input2 = analogRead(context, n/2, 2); // read in analog 2
        // read wet/dry mix from trimmer
        float input3 = analogRead(context, n/2, 3); // read in analog 3
        // volume control
        float input4 = analogRead(context, n/2, 4); // read in analog 4

        // map delay times to normal ranges - 1ms to 500ms
        delayLeft = map(input0, 0, 3.3 / 4.096, 0.01, 0.5);
        delayRight = map(input1, 0, 3.3 / 4.096, 0.01, 0.5);

        thirdPrevDelayL = secondPrevDelayL;
        secondPrevDelayL = prevDelayL;
        prevDelayL = delayLeft;
        thirdPrevDelayR = secondPrevDelayR;
        secondPrevDelayR = prevDelayR;
        prevDelayR = delayRight;

        prevDelayInSampsL = delayInSampsLeft;
        prevDelayInSampsR = delayInSampsRight;

        // map feedback and mix values to normal ranges (0 to 1)
        feedback = map(input2, 0, 3.3 / 4.096, 0, 0.75);
        wetFactor = map(input3, 0, 3.3 / 4.096, 0, 1.0);

        // map volume
        volume = map(input4, 0, 3.3 / 4.096, 0, 1);

        float dryFactor = 1.0f - wetFactor;

        // process input sample
        // change input based on button press
        if (currentButtonStatus == LOW && lastButtonStatus == HIGH) {
            in = gPlayer.process();
        } else {
            in = audioRead(context, n/2, 0);
        }
        lastButtonStatus = currentButtonStatus;

        // read at fractional place in delay buffers
        indexBelowLeft = floorf(readPointerLeft);
        indexAboveLeft = indexBelowLeft + 1;
        if(indexAboveLeft >= delayBufferLeftChannel.size())
            indexAboveLeft = 0;
        fractionAboveLeft = readPointerLeft - indexBelowLeft;
        fractionBelowLeft = 1.0 - fractionAboveLeft;

        float delayBufLeft = fractionBelowLeft * delayBufferLeftChannel[indexBelowLeft] + fractionAboveLeft * delayBufferLeftChannel[indexAboveLeft];

        indexBelowRight = floorf(readPointerRight);
        indexAboveRight = indexBelowRight + 1;
        if(indexAboveRight >= delayBufferRightChannel.size())
            indexAboveRight = 0;
        fractionAboveRight = readPointerRight - indexBelowRight;
        fractionBelowRight = 1.0 - fractionAboveRight;

        float delayBufRight = fractionBelowRight * delayBufferRightChannel[indexBelowRight] + fractionAboveRight * delayBufferRightChannel[indexAboveRight];

        // Read the output from the buffer, at the location expressed by the offset
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


        outLeft = lpLeft.process(outLeft);
        outRight = lpRight.process(outRight);

        // attenuate output for right now while we cut distortion
        outLeft *= volume;
        outRight *= volume;

        // Write the L and R different channels
        audioWrite(context, n, 0, outLeft);
        audioWrite(context, n, 1, outRight);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}