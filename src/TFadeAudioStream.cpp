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

#include "TFadeAudioStream.h"
#include "TRendererAudioStream.h"
#include "TAudioGlobals.h"
#include "UTools.h"

TFadeAudioStream::TFadeAudioStream(): TDecoratedAudioStream(0)
{
    fStatus = kIdle;
    fFadeInFrames = 0;
    fFadeOutFrames = 0;
    fCurFrame = 0;
    fFramesNum = 0;
    fMixBuffer = new TLocalAudioBuffer<float>(TAudioGlobals::fBufferSize, TAudioGlobals::fOutput);
}

TFadeAudioStream::TFadeAudioStream(TAudioStreamPtr stream, long fadeIn, long fadeOut): TDecoratedAudioStream(stream)
{
    fStatus = kFadeIn; // Starting state for the stream with a fade
    fFadeInFrames = fadeIn;
    fFadeOutFrames = fadeOut;
    fCurFrame = 0;
	fFramesNum = UTools::Max(0, fStream->Length() - fFadeOutFrames); // Number of frames - FadeOut 
    fMixBuffer = new TLocalAudioBuffer<float>(TAudioGlobals::fBufferSize, TAudioGlobals::fOutput);
    Init(0.0f, float(fadeIn), 1.0f, float(fadeOut));
}

long TFadeAudioStream::Read(FLOAT_BUFFER buffer, long framesNum, long framePos, long channels)
{
    switch (fStatus) {
        case kIdle:
            return 0;

        case kPlaying:
            return ReadAux(buffer, framesNum, framePos, channels);

        case kFadeIn:
            return FadeIn(buffer, framesNum, framePos, channels);

        case kFadeOut:
            return FadeOut(buffer, framesNum, framePos, channels);

        default:
            return 0;
    }
}

long TFadeAudioStream::ReadAux(FLOAT_BUFFER buffer, long framesNum, long framePos, long channels)
{
    long res = fStream->Read(buffer, framesNum, framePos, channels);
    fCurFrame += res;

    if (res < framesNum) { // should never happens
        fStatus = kIdle;
    } else if (fCurFrame >= fFramesNum) {
        fStatus = kFadeOut;
    }
    return res;
}

// TODO : start FadeOut while doing FadeIn

long TFadeAudioStream::FadeIn(FLOAT_BUFFER buffer, long framesNum, long framePos, long channels)
{
    UAudioTools::ZeroFloatBlk(fMixBuffer->GetFrame(0), framesNum, channels);
    long res = fStream->Read(fMixBuffer, framesNum, framePos, channels);
    fCurFrame += res;

    for (int i = framePos; i < framePos + framesNum; i++) {
        UAudioTools::MultFrame(fMixBuffer->GetFrame(i), fFadeIn.tick(), channels);
    }

    UAudioTools::MixFrameToFrameBlk(buffer->GetFrame(framePos),
                                    fMixBuffer->GetFrame(framePos),
                                    framesNum, channels);

    if (res < framesNum) {
        fStatus = kIdle;
    } else if (fFadeIn.lastOut() >= 1.0f) {
        fStatus = kPlaying;
    }

    return res;
}

long TFadeAudioStream::FadeOut(FLOAT_BUFFER buffer, long framesNum, long framePos, long channels)
{
    UAudioTools::ZeroFloatBlk(fMixBuffer->GetFrame(0), framesNum, channels);
    long res = fStream->Read(fMixBuffer, framesNum, framePos, channels);
    fCurFrame += res;

    for (int i = framePos; i < framePos + framesNum; i++) {
        UAudioTools::MultFrame(fMixBuffer->GetFrame(i), fFadeOut.tick(), channels);
    }

    UAudioTools::MixFrameToFrameBlk(buffer->GetFrame(framePos),
                                    fMixBuffer->GetFrame(framePos),
                                    framesNum, channels);

    if ((res < framesNum) || (fFadeOut.lastOut() <= 0.0f)) {
        fStatus = kIdle;
    }

    return res;
}

void TFadeAudioStream::Init(float fade_in_val, float fade_in_time, float fade_out_val, float fade_out_time)
{
    fFadeIn.setValue(fade_in_val);
    fFadeIn.setTarget(1.0f);
    fFadeIn.setTime(UAudioTools::ConvertFrameToSec(fade_in_time));

    fFadeOut.setValue(fade_out_val);
    fFadeOut.setTarget(0.0f);
    fFadeOut.setTime(UAudioTools::ConvertFrameToSec(fade_out_time));
}

/*
CutBegin(Fade (s, f1, f2), n)  ==> Fade (CutBegin (s, n) ,f1, f2) // A REVOIR
*/

TAudioStreamPtr TFadeAudioStream::CutBegin(long frames)
{
    // A FINIR
    return new TFadeAudioStream(fStream->CutBegin(frames), fFadeInFrames, fFadeOutFrames);
}

void TFadeAudioStream::Reset()
{
    fStream->Reset();
    fStatus = kFadeIn; // Starting state for the stream with a fade
	fCurFrame = 0;
    Init(0.0f, float(fFadeInFrames), 1.0f, float(fFadeOutFrames));
}

// Additional interface
void TChannelFadeAudioStream::SetStream(TAudioStreamPtr stream, long fadeIn, long fadeOut)
{
    fFadeInFrames = fadeIn;
    fFadeOutFrames = fadeOut;
    fStream = stream;
    fStatus = kIdle;
    fCurFrame = 0;
	fFramesNum = fStream->Length() - fFadeOutFrames; // Number of frames - FadeOut
    Init(0.0f, float(fFadeInFrames), 1.0f, float(fFadeOutFrames));
}

void TChannelFadeAudioStream::FadeIn()
{
    if (fCurFrame < fFramesNum) {
        fStatus = kFadeIn;
        Init(0.0f, float(fFadeInFrames), 1.0f, float(fFadeOutFrames));
    }
}

void TChannelFadeAudioStream::FadeOut()
{
    if (fCurFrame < fFramesNum) {
        fStatus = kFadeOut;
        Init(0.0f, float(fFadeInFrames), 1.0f, float(fFadeOutFrames));
    }
}



