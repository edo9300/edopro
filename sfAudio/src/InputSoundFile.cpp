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
#include "sfAudio/InputSoundFile.hpp"
#include "sfAudio/SoundFileReader.hpp"
#include "sfAudio/SoundFileFactory.hpp"
#include "sfAudio/System/InputStream.hpp"
#include "sfAudio/System/FileInputStream.hpp"
#include "sfAudio/System/MemoryInputStream.hpp"
#include <iostream>


namespace sf
{
////////////////////////////////////////////////////////////
InputSoundFile::InputSoundFile() :
m_reader      (NULL),
m_stream      (NULL),
m_streamOwned (false),
m_sampleOffset   (0),
m_sampleCount (0),
m_channelCount(0),
m_sampleRate  (0)
{
}


////////////////////////////////////////////////////////////
InputSoundFile::~InputSoundFile()
{
    // Close the file in case it was open
    close();
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromFile(const std::string& filename)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromFilename(filename);
    if (!m_reader)
        return false;

    // Wrap the file into a stream
    FileInputStream* file = new FileInputStream;
    m_stream = file;
    m_streamOwned = true;

    // Open it
    if (!file->open(filename))
    {
        close();
        return false;
    }

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(*file, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromMemory(const void* data, std::size_t sizeInBytes)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromMemory(data, sizeInBytes);
    if (!m_reader)
        return false;

    // Wrap the memory file into a stream
    MemoryInputStream* memory = new MemoryInputStream;
    m_stream = memory;
    m_streamOwned = true;

    // Open it
    memory->open(data, sizeInBytes);

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(*memory, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromStream(InputStream& stream)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromStream(stream);
    if (!m_reader)
        return false;

    // store the stream
    m_stream = &stream;
    m_streamOwned = false;

    // Don't forget to reset the stream to its beginning before re-opening it
    if (stream.seek(0) != 0)
    {
        std::cerr << "Failed to open sound file from stream (cannot restart stream)" << std::endl;
        return false;
    }

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(stream, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
uint64_t InputSoundFile::getSampleCount() const
{
    return m_sampleCount;
}


////////////////////////////////////////////////////////////
unsigned int InputSoundFile::getChannelCount() const
{
    return m_channelCount;
}


////////////////////////////////////////////////////////////
unsigned int InputSoundFile::getSampleRate() const
{
    return m_sampleRate;
}


////////////////////////////////////////////////////////////
Time InputSoundFile::getDuration() const
{
    // Make sure we don't divide by 0
    if (m_channelCount == 0 || m_sampleRate == 0)
        return Time::Zero;

    return seconds(static_cast<float>(m_sampleCount) / m_channelCount / m_sampleRate);
}


////////////////////////////////////////////////////////////
Time InputSoundFile::getTimeOffset() const
{
    // Make sure we don't divide by 0
    if (m_channelCount == 0 || m_sampleRate == 0)
        return Time::Zero;

    return seconds(static_cast<float>(m_sampleOffset) / m_channelCount / m_sampleRate);
}


////////////////////////////////////////////////////////////
uint64_t InputSoundFile::getSampleOffset() const
{
    return m_sampleOffset;
}


////////////////////////////////////////////////////////////
void InputSoundFile::seek(uint64_t sampleOffset)
{
    if (m_reader)
    {
        // The reader handles an overrun gracefully, but we
        // pre-check to keep our known position consistent
        m_sampleOffset = std::min(sampleOffset, m_sampleCount);
        m_reader->seek(m_sampleOffset);
    }
}


////////////////////////////////////////////////////////////
void InputSoundFile::seek(Time timeOffset)
{
    seek(static_cast<uint64_t>(timeOffset.asSeconds() * m_sampleRate * m_channelCount));
}


////////////////////////////////////////////////////////////
uint64_t InputSoundFile::read(int16_t* samples, uint64_t maxCount)
{
    uint64_t readSamples = 0;
    if (m_reader && samples && maxCount)
        readSamples = m_reader->read(samples, maxCount);
    m_sampleOffset += readSamples;
    return readSamples;
}


////////////////////////////////////////////////////////////
void InputSoundFile::close()
{
    // Destroy the reader
    delete m_reader;
    m_reader = NULL;

    // Destroy the stream if we own it
    if (m_streamOwned)
    {
        delete m_stream;
        m_streamOwned = false;
    }
    m_stream = NULL;
    m_sampleOffset = 0;

    // Reset the sound file attributes
    m_sampleCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;
}

} // namespace sf
