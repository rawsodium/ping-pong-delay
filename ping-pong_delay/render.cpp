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

// declare variables for circular buffer
std::vector<float> gDelayBuffer;
unsigned int gWritePointer = 0;
unsigned int gOffset = 0;

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
    			
    // allocate the circular buffer to contain enough samples to cover 0.5 seconds
    gDelayBuffer.resize(0.5 * context->audioSampleRate);
    
    // TODO Express the 0.1 s delay as an offset from the write pointer (oldest sample) measured in samples. 
    // the more samples, the more delay
    gOffset = 0.1 * context->audioSampleRate;

	return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        float in = gPlayer.process();
      
    	// TODO Read the output from the buffer, at the location expressed by the offset
        float out = gDelayBuffer[(gWritePointer - gOffset + gDelayBuffer.size()) % gDelayBuffer.size()]; // use modulo to browse circular buffer!
        // in the case of an offset equivalent to 0.5 s (max delay), 
        // the modulo will wrap back the value to the original gWritePointer position (oldest sample)
        
        // Update the circular buffer
        gDelayBuffer[gWritePointer] = in; // overwrite the buffer at the write pointer
        gWritePointer++; // move pointer
        // wrap pointer
        if(gWritePointer >= gDelayBuffer.size())
        	gWritePointer = 0;
        
		// Write the input and output to different channels
    	audioWrite(context, n, 0, in);
    	audioWrite(context, n, 1, out);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
