/*
Copyright (C) Grame 2002-2013

This library is free software; you can redistribute it and modify it under
the terms of the GNU Library General Public License as published by the
Free Software Foundation version 2 of the License, or any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public License
for more details.

You should have received a copy of the GNU Library General Public License
along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Grame Research Laboratory, 9, rue du Garet 69001 Lyon - France
research@grame.fr

*/

#include "TReadFileAudioStream.h"
#include "TCmdManager.h"
#include "TAudioGlobals.h"
#include "UAudioTools.h"
#include "UTools.h"
#include "StringTools.h"
#include <stdio.h>
#include <assert.h>

TReadFileAudioStream::TReadFileAudioStream(string name, long beginFrame): TFileAudioStream(name)
{
  	memset(&fInfo, 0, sizeof(fInfo));
	char utf8name[512] = {0};
	
	assert(fName.size() < 512);
	Convert2UTF8(fName.c_str(), utf8name, 512);
	fFile = sf_open(utf8name, SFM_READ, &fInfo);
	
    // Check file
    if (!fFile) {
        throw - 1;
    }

    if (sf_seek(fFile, beginFrame, SEEK_SET) < 0) {
        sf_close(fFile);
        throw - 2;
    }

    fFramesNum = long(fInfo.frames);
    fChannels = long(fInfo.channels);
    fBeginFrame = beginFrame;

	// Needed because we later on use sf_readf_short, should be removed is sf_readf_float is used instead.
    if (fInfo.format & SF_FORMAT_FLOAT) {
        int arg = SF_TRUE;
        sf_command(fFile, SFC_SET_SCALE_FLOAT_INT_READ, &arg, sizeof(arg));
    }

    if (fInfo.samplerate != TAudioGlobals::fSampleRate) {
        printf("Warning : file sample rate different from engine sample rate! lib sr = %ld file sr = %d\n", TAudioGlobals::fSampleRate, fInfo.samplerate);
    }

    // Dynamic allocation
    fMemoryBuffer = new TLocalAudioBuffer<short>(TAudioGlobals::fStreamBufferSize, fChannels);
    fCopyBuffer = new TLocalAudioBuffer<short>(TAudioGlobals::fStreamBufferSize, fChannels);
  
    // Read first buffer directly
    TBufferedAudioStream::ReadBuffer(fMemoryBuffer, TAudioGlobals::fStreamBufferSize, 0);
    TAudioBuffer<short>::Copy(fCopyBuffer, 0, fMemoryBuffer, 0, TAudioGlobals::fStreamBufferSize);

    fReady = true;
}

TReadFileAudioStream::~TReadFileAudioStream()
{
	if (fFile) {
        sf_close(fFile);
        fFile = 0;
    }

    delete fMemoryBuffer;
    delete fCopyBuffer;
}

TAudioStreamPtr TReadFileAudioStream::CutBegin(long frames)
{
    return new TReadFileAudioStream(fName, fBeginFrame + frames);
}

void TReadFileAudioStream::ReadEndBufferAux(TReadFileAudioStreamPtr obj, long framesNum, long framePos)
{
    obj->ReadEndBuffer(framesNum, framePos);
}

// Use the end of the copy buffer
void TReadFileAudioStream::ReadEndBuffer(long framesNum, long framePos)
{
    TAudioBuffer<short>::Copy(fMemoryBuffer, framePos, fCopyBuffer, framePos, framesNum);
}

void TReadFileAudioStream::Reset()
{
    if (sf_seek(fFile, fBeginFrame + TAudioGlobals::fStreamBufferSize, SEEK_SET) < 0) {
        printf("TReadFileAudioStream::Reset : sf_seek error = %s\n", sf_strerror(fFile));
    }

    // Use only the beginning of the copy buffer, copy the end in the low-priority thread
    int copySize = TAudioGlobals::fBufferSize * 4;

    if (copySize < TAudioGlobals::fStreamBufferSize) {
        TAudioBuffer<short>::Copy(fMemoryBuffer, 0, fCopyBuffer, 0, copySize);
        if (fManager == 0) {
            printf("Error : stream rendered without command manager\n");
        }
        assert(fManager);
        fManager->ExecCmd((CmdPtr)ReadEndBufferAux, (long)this, TAudioGlobals::fStreamBufferSize - copySize, copySize, 0, 0);
    } else {
        TAudioBuffer<short>::Copy(fMemoryBuffer, 0, fCopyBuffer, 0, TAudioGlobals::fStreamBufferSize);
    }

    TBufferedAudioStream::Reset();
}

// Called by TCmdManager
long TReadFileAudioStream::Read(SHORT_BUFFER buffer, long framesNum, long framePos)
{
    assert(fFile);
    return long(sf_readf_short(fFile, buffer->GetFrame(framePos), framesNum)); // In frames
}


