////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2019 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "sfAudio/SoundStream.hpp"
#include "sfAudio/AudioDevice.hpp"
#include "sfAudio/ALCheck.hpp"
#include <iostream>

#ifdef _MSC_VER
    #pragma warning(disable: 4355) // 'this' used in base member initializer list
#endif


namespace sf
{
////////////////////////////////////////////////////////////
SoundStream::SoundStream() :
m_threadMutex     (),
m_threadStartState(Stopped),
m_isStreaming     (false),
m_buffers         (),
m_channelCount    (0),
m_sampleRate      (0),
m_format          (0),
m_loop            (false),
m_samplesProcessed(0),
m_bufferSeeks     ()
{

}


////////////////////////////////////////////////////////////
SoundStream::~SoundStream()
{
    // Stop the sound if it was playing

    // Request the thread to terminate
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_isStreaming = false;
    }

    // Wait for the thread to terminate
	if(m_thread.joinable())
		m_thread.join();
}


////////////////////////////////////////////////////////////
void SoundStream::initialize(unsigned int channelCount, unsigned int sampleRate)
{
    m_channelCount = channelCount;
    m_sampleRate = sampleRate;
    m_samplesProcessed = 0;
    m_isStreaming = false;

    // Deduce the format from the number of channels
    m_format = priv::AudioDevice::getFormatFromChannelCount(channelCount);

    // Check if the format is valid
    if (m_format == 0)
    {
        m_channelCount = 0;
        m_sampleRate   = 0;
        std::cerr << "Unsupported number of channels (" << m_channelCount << ")" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void SoundStream::play()
{
    // Check if the sound parameters have been set
    if (m_format == 0)
    {
        std::cerr << "Failed to play audio stream: sound parameters have not been initialized (call initialize() first)" << std::endl;
        return;
    }

    bool isStreaming = false;
    Status threadStartState = Stopped;

    {
        std::lock_guard<std::mutex> lock(m_threadMutex);

        isStreaming = m_isStreaming;
        threadStartState = m_threadStartState;
    }


    if (isStreaming && (threadStartState == Paused))
    {
        // If the sound is paused, resume it
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_threadStartState = Playing;
        alCheck(alSourcePlay(m_source));
        return;
    }
    else if (isStreaming && (threadStartState == Playing))
    {
        // If the sound is playing, stop it and continue as if it was stopped
        stop();
    }

    // Start updating the stream in a separate thread to avoid blocking the application
    m_isStreaming = true;
    m_threadStartState = Playing;
	m_thread = std::thread(&SoundStream::streamData, this);
}


////////////////////////////////////////////////////////////
void SoundStream::pause()
{
    // Handle pause() being called before the thread has started
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);

        if (!m_isStreaming)
            return;

        m_threadStartState = Paused;
    }

    alCheck(alSourcePause(m_source));
}


////////////////////////////////////////////////////////////
void SoundStream::stop()
{
    // Request the thread to terminate
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_isStreaming = false;
    }

    // Wait for the thread to terminate
	if(m_thread.joinable())
		m_thread.join();

    // Move to the beginning
    onSeek(Time::Zero);
}


////////////////////////////////////////////////////////////
unsigned int SoundStream::getChannelCount() const
{
    return m_channelCount;
}


////////////////////////////////////////////////////////////
unsigned int SoundStream::getSampleRate() const
{
    return m_sampleRate;
}


////////////////////////////////////////////////////////////
SoundStream::Status SoundStream::getStatus() const
{
    Status status = SoundSource::getStatus();

    // To compensate for the lag between play() and alSourceplay()
    if (status == Stopped)
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);

        if (m_isStreaming)
            status = m_threadStartState;
    }

    return status;
}


////////////////////////////////////////////////////////////
void SoundStream::setPlayingOffset(Time timeOffset)
{
    // Get old playing status
    Status oldStatus = getStatus();

    // Stop the stream
    stop();

    // Let the derived class update the current position
    onSeek(timeOffset);

    // Restart streaming
    m_samplesProcessed = static_cast<uint64_t>(timeOffset.asSeconds() * m_sampleRate * m_channelCount);

    if (oldStatus == Stopped)
        return;

    m_isStreaming = true;
    m_threadStartState = oldStatus;
	m_thread = std::thread(&SoundStream::streamData, this);
}


////////////////////////////////////////////////////////////
Time SoundStream::getPlayingOffset() const
{
    if (m_sampleRate && m_channelCount)
    {
        ALfloat secs = 0.f;
        alCheck(alGetSourcef(m_source, AL_SEC_OFFSET, &secs));

        return seconds(secs + static_cast<float>(m_samplesProcessed) / m_sampleRate / m_channelCount);
    }
    else
    {
        return Time::Zero;
    }
}


////////////////////////////////////////////////////////////
void SoundStream::setLoop(bool loop)
{
    m_loop = loop;
}


////////////////////////////////////////////////////////////
bool SoundStream::getLoop() const
{
    return m_loop;
}


////////////////////////////////////////////////////////////
int64_t SoundStream::onLoop()
{
    onSeek(Time::Zero);
    return 0;
}


////////////////////////////////////////////////////////////
void SoundStream::streamData()
{
    bool requestStop = false;

    {
        std::lock_guard<std::mutex> lock(m_threadMutex);

        // Check if the thread was launched Stopped
        if (m_threadStartState == Stopped)
        {
            m_isStreaming = false;
            return;
        }
    }

    // Create the buffers
    alCheck(alGenBuffers(BufferCount, m_buffers));
    for (int i = 0; i < BufferCount; ++i)
        m_bufferSeeks[i] = NoLoop;

    // Fill the queue
    requestStop = fillQueue();

    // Play the sound
    alCheck(alSourcePlay(m_source));

    {
        std::lock_guard<std::mutex> lock(m_threadMutex);

        // Check if the thread was launched Paused
        if (m_threadStartState == Paused)
            alCheck(alSourcePause(m_source));
    }

    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(m_threadMutex);
            if (!m_isStreaming)
                break;
        }

        // The stream has been interrupted!
        if (SoundSource::getStatus() == Stopped)
        {
            if (!requestStop)
            {
                // Just continue
                alCheck(alSourcePlay(m_source));
            }
            else
            {
                // End streaming
                std::lock_guard<std::mutex> lock(m_threadMutex);
                m_isStreaming = false;
            }
        }

        // Get the number of buffers that have been processed (i.e. ready for reuse)
        ALint nbProcessed = 0;
        alCheck(alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &nbProcessed));

        while (nbProcessed--)
        {
            // Pop the first unused buffer from the queue
            ALuint buffer;
            alCheck(alSourceUnqueueBuffers(m_source, 1, &buffer));

            // Find its number
            unsigned int bufferNum = 0;
            for (int i = 0; i < BufferCount; ++i)
                if (m_buffers[i] == buffer)
                {
                    bufferNum = i;
                    break;
                }

            // Retrieve its size and add it to the samples count
            if (m_bufferSeeks[bufferNum] != NoLoop)
            {
                // This was the last buffer before EOF or Loop End: reset the sample count
                m_samplesProcessed = m_bufferSeeks[bufferNum];
                m_bufferSeeks[bufferNum] = NoLoop;
            }
            else
            {
                ALint size, bits;
                alCheck(alGetBufferi(buffer, AL_SIZE, &size));
                alCheck(alGetBufferi(buffer, AL_BITS, &bits));

                // Bits can be 0 if the format or parameters are corrupt, avoid division by zero
                if (bits == 0)
                {
                    std::cerr << "Bits in sound stream are 0: make sure that the audio format is not corrupt "
                          << "and initialize() has been called correctly" << std::endl;

                    // Abort streaming (exit main loop)
                    std::lock_guard<std::mutex> lock(m_threadMutex);
                    m_isStreaming = false;
                    requestStop = true;
                    break;
                }
                else
                {
                    m_samplesProcessed += size / (bits / 8);
                }
            }

            // Fill it and push it back into the playing queue
            if (!requestStop)
            {
                if (fillAndPushBuffer(bufferNum))
                    requestStop = true;
            }
        }

        // Leave some time for the other threads if the stream is still playing
        if (SoundSource::getStatus() != Stopped)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Stop the playback
    alCheck(alSourceStop(m_source));

    // Dequeue any buffer left in the queue
    clearQueue();

    // Reset the playing position
    m_samplesProcessed = 0;

    // Delete the buffers
    alCheck(alSourcei(m_source, AL_BUFFER, 0));
    alCheck(alDeleteBuffers(BufferCount, m_buffers));
}


////////////////////////////////////////////////////////////
bool SoundStream::fillAndPushBuffer(unsigned int bufferNum, bool immediateLoop)
{
    bool requestStop = false;

    // Acquire audio data, also address EOF and error cases if they occur
    Chunk data = {NULL, 0};
    for (uint32_t retryCount = 0; !onGetData(data) && (retryCount < BufferRetries); ++retryCount)
    {
        // Check if the stream must loop or stop
        if (!m_loop)
        {
            // Not looping: Mark this buffer as ending with 0 and request stop
            if (data.samples != NULL && data.sampleCount != 0)
                m_bufferSeeks[bufferNum] = 0;
            requestStop = true;
            break;
        }

        // Return to the beginning or loop-start of the stream source using onLoop(), and store the result in the buffer seek array
        // This marks the buffer as the "last" one (so that we know where to reset the playing position)
        m_bufferSeeks[bufferNum] = onLoop();

        // If we got data, break and process it, else try to fill the buffer once again
        if (data.samples != NULL && data.sampleCount != 0)
            break;

        // If immediateLoop is specified, we have to immediately adjust the sample count
        if (immediateLoop && (m_bufferSeeks[bufferNum] != NoLoop))
        {
            // We just tried to begin preloading at EOF or Loop End: reset the sample count
            m_samplesProcessed = m_bufferSeeks[bufferNum];
            m_bufferSeeks[bufferNum] = NoLoop;
        }

        // We're a looping sound that got no data, so we retry onGetData()
    }

    // Fill the buffer if some data was returned
    if (data.samples && data.sampleCount)
    {
        unsigned int buffer = m_buffers[bufferNum];

        // Fill the buffer
        ALsizei size = static_cast<ALsizei>(data.sampleCount) * sizeof(int16_t);
        alCheck(alBufferData(buffer, m_format, data.samples, size, m_sampleRate));

        // Push it into the sound queue
        alCheck(alSourceQueueBuffers(m_source, 1, &buffer));
    }
    else
    {
        // If we get here, we most likely ran out of retries
        requestStop = true;
    }

    return requestStop;
}


////////////////////////////////////////////////////////////
bool SoundStream::fillQueue()
{
    // Fill and enqueue all the available buffers
    bool requestStop = false;
    for (int i = 0; (i < BufferCount) && !requestStop; ++i)
    {
        // Since no sound has been loaded yet, we can't schedule loop seeks preemptively,
        // So if we start on EOF or Loop End, we let fillAndPushBuffer() adjust the sample count
        if (fillAndPushBuffer(i, (i == 0)))
            requestStop = true;
    }

    return requestStop;
}


////////////////////////////////////////////////////////////
void SoundStream::clearQueue()
{
    // Get the number of buffers still in the queue
    ALint nbQueued;
    alCheck(alGetSourcei(m_source, AL_BUFFERS_QUEUED, &nbQueued));

    // Dequeue them all
    ALuint buffer;
    for (ALint i = 0; i < nbQueued; ++i)
        alCheck(alSourceUnqueueBuffers(m_source, 1, &buffer));
}

} // namespace sf
