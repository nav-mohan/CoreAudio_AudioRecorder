typedef struct MyRecorder{
    AudioFileID recordFile;
    SInt64 recordPacket;
    Boolean running;
} MyRecorder;

static void CheckError(OSStatus error, const char* operation){
    if (error == noErr) return;
    
    char errorString[20];
    //see if it appears to be a 4-char code
    *(UInt32 *)(errorString + 1) = CFSwapInt32HostToBig(error);
    if (isprint(errorString[1]) && isprint(errorString[2]) && isprint(errorString[3]) && isprint(errorString[4])) {
        errorString[0] = errorString[5] = '\'';
        errorString[6] = '\0';
    }
    else
        sprintf(errorString, "%d", (int)error);
    fprintf(stderr, "Error: %s (%s)\n", operation, errorString);
    exit(1);
}

OSStatus MyGetDefaultInputDeviceSampleRate(Float64* outSampleRate){
    OSStatus error;
    AudioDeviceID deviceID = 0;
    
    AudioObjectPropertyAddress propertyAddress;
    UInt32 propertySize;
    
    // First we have to find the deviceID
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = 0;
    propertySize = sizeof(AudioDeviceID);
    error = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &deviceID);
    if (error) return error;
    
    // Now we have the deviceID, we can ask for the device's sample rate
    propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = 0;
    propertySize = sizeof(Float64);
    error = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &propertySize, outSampleRate);
    return error;
}

static void MyCopyEncoderCookieToFile(AudioQueueRef queue, AudioFileID theFile){
    OSStatus error;
    UInt32 propertySize;
    
    error = AudioQueueGetPropertySize(queue, kAudioConverterCompressionMagicCookie, &propertySize);
    
    if (error == noErr && propertySize > 0){
        Byte* magicCookie = (Byte*)malloc(propertySize);
        CheckError(AudioQueueGetProperty(queue, kAudioQueueProperty_MagicCookie, magicCookie, &propertySize), "Couldn't get audio queue's magic cookie");
        CheckError(AudioFileSetProperty(theFile, kAudioFilePropertyMagicCookieData, propertySize, magicCookie), "Couldn't set audio file's magic cookie");
        free(magicCookie);
    }
}

static int MyComputeRecordBufferSize(const AudioStreamBasicDescription* ASBD, AudioQueueRef queue, float seconds){
    int packets, frames, bytes;
    frames = (int)ceil(seconds * ASBD->mSampleRate);
    if (ASBD->mBytesPerFrame > 0) //  UNCOMPRESSED FORMATS (WAV,AIFF)
        bytes = frames * ASBD->mSampleRate;
    else{// COMPRESSED FORMAT (MP3,AAC)
        UInt32 maxPacketSize;
        if(ASBD->mBytesPerPacket > 0) // CONSTANT PACKET SIZE
        {
            maxPacketSize = ASBD->mBytesPerPacket;
            printf("@MyComputeRecordBufferSize: CONSTANT PACKET SIZE OF %d\n",maxPacketSize);
        }
        else{// if every packet is of a different size then just get the theoertically largest possible packet size
            UInt32 propertySize = sizeof(maxPacketSize);
            CheckError(AudioQueueGetProperty(queue, kAudioConverterPropertyMaximumOutputPacketSize, &maxPacketSize, &propertySize),"Couldn't get kAudioConverterPropertyMaximumOutputPacketSize");
            printf("@MyComputeRecordBufferSize: MAX-PACKET-SIZE = %d\n",maxPacketSize);
        }
        if (packets==0)
            packets=1;
        bytes = packets * maxPacketSize;
    }
    printf("@MyComputeRecordBufferSize: packets= %d\n",packets);
    printf("@MyComputeRecordBufferSize: bytes= %d\n",bytes);
    return bytes;
}

static void MYAQInputCallback(void* inUserData, AudioQueueRef inQueue, AudioQueueBufferRef inBuffer, const AudioTimeStamp* inStartTime, UInt32 inNumpackets, const AudioStreamPacketDescription* inPacketDesc){
    printf("@MYAQInputCallback : inNumpackets = %d\n",inNumpackets);
    MyRecorder* recorder = (MyRecorder*)inUserData;
    if (inNumpackets>0){
        CheckError(
            AudioFileWritePackets(
                recorder->recordFile, 
                FALSE, 
                inBuffer->mAudioDataByteSize, 
                inPacketDesc, 
                recorder->recordPacket, 
                &inNumpackets, 
                inBuffer->mAudioData
            ),
            "@MYAQInputCallback: AudioFileWritePackets failed"
        );
        
        recorder->recordPacket+=inNumpackets;
        
        if (recorder->running)
            // RE-ENQUEUE BUFFER
            CheckError(AudioQueueEnqueueBuffer(inQueue, inBuffer, 0, NULL),"@MYAQInputCallback: AudioQueueEnqueueBuffer Failed");
    }
}
