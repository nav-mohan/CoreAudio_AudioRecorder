#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioConverter.h>
#include <CoreAudio/AudioHardwareBase.h>//AudioObjectPropertyAddress
#include <CoreFoundation/CoreFoundation.h>
#include "utilFuncs.h"

int main(){
	MyRecorder recorder = {0};
	AudioStreamBasicDescription asbd;
	memset(&asbd, 0, sizeof(asbd));
	asbd.mFormatID = kAudioFormatMPEG4AAC;
	asbd.mChannelsPerFrame = 2;
    
    // FIND DEFAULT-DEVICE'S SAMPLE-RATE AND WRITE IT INTO asbd.mSampleRate    
    CheckError(MyGetDefaultInputDeviceSampleRate(&asbd.mSampleRate),"Failed to get Default Input Devie Sample Rate");

    // 
    UInt32 propSize = sizeof(asbd);
    CheckError(AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &propSize, &asbd), "Failed to get format info of the ASBD");
    
    // CREATE INPUT-QUEUE AND FILL IN INPUT-QUEUE'S FORMAT PROPERTY
    AudioQueueRef queue = {0};
    CheckError(AudioQueueNewInput(&asbd, MYAQInputCallback, &recorder, NULL,NULL, 0, &queue),"Failed to Create New Input Queue");
    UInt32 size = sizeof(asbd);
    CheckError(AudioQueueGetProperty(queue,kAudioConverterCurrentOutputStreamDescription,&asbd,&size),"Couldn't get queue's format");

    // CREATE FILE
    CFURLRef myFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("output.caf"), kCFURLPOSIXPathStyle, false);
    CheckError(AudioFileCreateWithURL(myFileURL, kAudioFileCAFType, &asbd, kAudioFileFlags_EraseFile, &recorder.recordFile), "Failed to Create Auido File");
    CFRelease(myFileURL);

    //COPY MAGIC-COOKIE FROM INPUT-QUEUE TO FILE
    MyCopyEncoderCookieToFile(queue, recorder.recordFile);

    // FIND RECORDING BUFFER'S SIZE (DEPENDS ON asbd, queueu, duration)
    int bufferByteSize = MyComputeRecordBufferSize(&asbd,queue,0.5);

    // ALLOCATING AND ENQUEUEING BUFFERS
    int bufferIndex;
    for(bufferIndex=0; bufferIndex < 3; ++bufferIndex){
        AudioQueueBufferRef buffer;
        CheckError(AudioQueueAllocateBuffer(queue, bufferByteSize, &buffer),"Failed to allocate buffer");
        CheckError(AudioQueueEnqueueBuffer(queue,buffer,0, NULL),"Failed to Enqueue Buffer");
    }

    // START,STOP,DISPOSE QUEUE
    // 1) START QUEUE
    recorder.running = TRUE;
    printf("Press <ENTER> to START recording...\n");
    getchar();
    CheckError(AudioQueueStart(queue,NULL),"Failed to start queue");
    // 2) STOP QUEUE
    printf("Press <ENTER> to STOP recording...\n");
    getchar();
    printf("Done recording\n");
    recorder.running =  FALSE;
    CheckError(AudioQueueStop(queue,TRUE),"Failed to stop AuqioQueue");
    // 3a) ONCE AGAIN WRITE THE MAGIC-COOKIE FROMO INPUT-QUEUE TO AUDIO FILE
    MyCopyEncoderCookieToFile(queue,recorder.recordFile);
    // 3b) DISPOSE AudioQueue
    CheckError(AudioQueueDispose(queue,TRUE),"Failed to Dispose Input Queue");
    CheckError(AudioFileClose(recorder.recordFile),"Failed to close audio file");
	
    return 0;
}