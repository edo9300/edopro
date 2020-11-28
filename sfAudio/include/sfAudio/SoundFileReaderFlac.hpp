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

#ifndef SFML_SOUNDFILEREADERFLAC_HPP
#define SFML_SOUNDFILEREADERFLAC_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "sfAudio/SoundFileReader.hpp"
#include <FLAC/stream_decoder.h>
#include <string>
#include <vector>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Implementation of sound file reader that handles FLAC files
///
////////////////////////////////////////////////////////////
class SoundFileReaderFlac : public SoundFileReader
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Check if this reader can handle a file given by an input stream
    ///
    /// \param stream Source stream to check
    ///
    /// \return True if the file is supported by this reader
    ///
    ////////////////////////////////////////////////////////////
    static bool check(InputStream& stream);

public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    SoundFileReaderFlac();

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    ~SoundFileReaderFlac();

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file for reading
    ///
    /// \param stream Stream to open
    /// \param info   Structure to fill with the attributes of the loaded sound
    ///
    ////////////////////////////////////////////////////////////
    virtual bool open(sf::InputStream& stream, Info& info);

    ////////////////////////////////////////////////////////////
    /// \brief Change the current read position to the given sample offset
    ///
    /// The sample offset takes the channels into account.
    /// If you have a time offset instead, you can easily find
    /// the corresponding sample offset with the following formula:
    /// `timeInSeconds * sampleRate * channelCount`
    /// If the given offset exceeds to total number of samples,
    /// this function must jump to the end of the file.
    ///
    /// \param sampleOffset Index of the sample to jump to, relative to the beginning
    ///
    ////////////////////////////////////////////////////////////
    virtual void seek(uint64_t sampleOffset);

    ////////////////////////////////////////////////////////////
    /// \brief Read audio samples from the open file
    ///
    /// \param samples  Pointer to the sample array to fill
    /// \param maxCount Maximum number of samples to read
    ///
    /// \return Number of samples actually read (may be less than \a maxCount)
    ///
    ////////////////////////////////////////////////////////////
    virtual uint64_t read(int16_t* samples, uint64_t maxCount);

public:

    ////////////////////////////////////////////////////////////
    /// \brief Hold the state that is passed to the decoder callbacks
    ///
    ////////////////////////////////////////////////////////////
    struct ClientData
    {
        InputStream*          stream;
        SoundFileReader::Info info;
		int16_t*                buffer;
		uint64_t                remaining;
        std::vector<int16_t>    leftovers;
        bool                  error;
    };

private:

    ////////////////////////////////////////////////////////////
    /// \brief Close the open FLAC file
    ///
    ////////////////////////////////////////////////////////////
    void close();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    FLAC__StreamDecoder* m_decoder;    //!< FLAC decoder
    ClientData           m_clientData; //!< Structure passed to the decoder callbacks
};

} // namespace priv

} // namespace sf


#endif // SFML_SOUNDFILEREADERFLAC_HPP
