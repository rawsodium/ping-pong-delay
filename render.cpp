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
float readPointerLeft = 0.0;
float readPointerRight = 0.0;

// for interpolation of both channels
int indexAboveLeft = 0;
int indexAboveRight = 0;
int indexBelowLeft = 0;
int indexBelowRight = 0;
float fractionAboveLeft = 0.0;
float fractionAboveRight = 0.0;
float fractionBelowLeft = 0.0;
float fractionBelowRight = 0.0;

// declare variable for input up here so it is initialized
float in = 0.0;

// store current and last button states
int currentButtonStatus = HIGH;
int lastButtonStatus = HIGH;

// button pin
const int kButtonPin = 0;

// lowpass filter
Biquad lowPass;

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

    // settings for LPF
    Biquad::Settings settings{ .fs = context->audioSampleRate, .type = Biquad::lowpass};

    lowPass.setup(settings);
    lowPass.setQ(0.707);
    lowPass.setPeakGain(1.0);

    lowPass.setFc(10.0);

    return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // check for a button press
        currentButtonStatus = digitalRead(context, n, kButtonPin);

        // read delay time from trimmer
        float input0 = analogRead(context, n/2, 0); // read in analog 0
        input0 = lowPass.process(input0);

        // read feedback from trimmer
        float input1 = analogRead(context, n/2, 1); // read in analog 1
        // read wet/dry mix from trimmer
        float input2 = analogRead(context, n/2, 2); // read in analog 2
        // volume control
        float input3 = analogRead(context, n/2, 3); // read in analog 3
        // wet gain control
        float input4 = analogRead(context, n/2, 4); // read in analog 4

        // map delay time to normal range - 10ms to 500ms
        float delayTime = map(input0, 0, 3.3 / 4.096, 0.01, 0.5);

        // map feedback, mix, volume, and wet gain
        float feedback = map(input1, 0, 3.3 / 4.096, 0, 0.75);
        float wetFactor = map(input2, 0, 3.3 / 4.096, 0, 1.0);
        float volume = map(input3, 0, 3.3 / 4.096, 0, 1);
        float gain_wet = map(input4, 0, 3.3 / 4.096, 0.5, 1.0);

        float dryFactor = 1.0f - wetFactor;

        // change input based on button press
        // default: direct audio input, button press = pre-loaded sound file
        if (currentButtonStatus == LOW) {
            in = gPlayer.process();
        } else {
            in = audioRead(context, n, 0);
        }

        // reset button status
        lastButtonStatus = currentButtonStatus;

        float delayInSampsLeft = delayTime * context->audioSampleRate;
        float delayInSampsRight = delayTime * context->audioSampleRate;

        readPointerLeft = fmodf( (float)writePointerLeft - (float)delayInSampsLeft + (float)delayBufferLeftChannel.size(), (float)delayBufferLeftChannel.size() );
        readPointerRight = fmodf( (float)writePointerRight - (float) delayInSampsRight + (float)delayBufferRightChannel.size(), (float) delayBufferRightChannel.size());

        // read at fractional place in delay buffers
        indexBelowLeft = floorf(readPointerLeft);
        indexAboveLeft = indexBelowLeft + 1;
        if(indexAboveLeft >= delayBufferLeftChannel.size())
            indexAboveLeft = 0;
        fractionAboveLeft = readPointerLeft - indexBelowLeft;
        fractionBelowLeft = 1.0 - fractionAboveLeft;

        float delayedOutputLeft = fractionBelowLeft * delayBufferLeftChannel[indexBelowLeft] + fractionAboveLeft * delayBufferLeftChannel[indexAboveLeft];

        indexBelowRight = floorf(readPointerRight);
        indexAboveRight = indexBelowRight + 1;
        if(indexAboveRight >= delayBufferRightChannel.size())
            indexAboveRight = 0;
        fractionAboveRight = readPointerRight - indexBelowRight;
        fractionBelowRight = 1.0 - fractionAboveRight;

        float delayedOutputRight = fractionBelowRight * delayBufferRightChannel[indexBelowRight] + fractionAboveRight * delayBufferRightChannel[indexAboveRight];


        float delayedInputLeft = in + delayedOutputRight * feedback;
        float delayedInputRight = delayedOutputLeft * feedback;

        // write input and feedback to buffers
        delayBufferLeftChannel[writePointerLeft] = delayedInputLeft;
        delayBufferRightChannel[writePointerRight] = delayedInputRight;

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

        // Read the output from the buffer, at the location expressed by the offset
        float outLeft = in * dryFactor + delayedOutputLeft * wetFactor;
        float outRight = in * dryFactor + delayedOutputRight * wetFactor;

        // attenuate output for right now while we cut distortion
        outLeft *= volume;
        outRight *= (volume * gain_wet);

        // Write the L and R different channels
        audioWrite(context, n, 0, outLeft);
        audioWrite(context, n, 1, outRight);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}