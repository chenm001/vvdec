/* -----------------------------------------------------------------------------
The copyright in this software is being made available under the BSD
License, included below. No patent rights, trademark rights and/or 
other Intellectual Property Rights other than the copyrights concerning 
the Software are granted under this license.

For any license concerning other Intellectual Property rights than the software, 
especially patent licenses, a separate Agreement needs to be closed. 
For more information please contact:

Fraunhofer Heinrich Hertz Institute
Einsteinufer 37
10587 Berlin, Germany
www.hhi.fraunhofer.de/vvc
vvc@hhi.fraunhofer.de

Copyright (c) 2018-2020, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of Fraunhofer nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.


------------------------------------------------------------------------------------------- */

/** \file     BinDecoder.h
 *  \brief    Low level binary symbol writer
 */

#pragma once


#include "CommonLib/Contexts.h"
#include "CommonLib/BitStream.h"

class BinDecoderBase : public Ctx
{
protected:
  BinDecoderBase()
    : Ctx         ()
    , m_Bitstream ( 0 )
    , m_Range     ( 0 )
    , m_Value     ( 0 )
    , m_bitsNeeded( 0 )
  {}
public:
  ~BinDecoderBase() {}

public:
  void init( InputBitstream* bitstream )    { m_Bitstream = bitstream;  }
  void uninit()                             { m_Bitstream = 0;          }
  void start()
  {
    CHECK( m_Bitstream->getNumBitsUntilByteAligned(), "Bitstream is not byte aligned." );
    m_Range       = 510;
    m_Value       = ( m_Bitstream->readByte() << 8 ) + m_Bitstream->readByte();
    m_bitsNeeded  = -8;
  }
  void finish()
  {
    unsigned lastByte;
    m_Bitstream->peekPreviousByte( lastByte );
    CHECK( ( ( lastByte << ( 8 + m_bitsNeeded ) ) & 0xff ) != 0x80,
           "No proper stop/alignment pattern at end of CABAC stream." );
  }
  void reset( int qp, int initId )
  {
    Ctx::init( qp, initId );
    start();
  }

public:
  unsigned          decodeBinEP         ();
  unsigned          decodeBinsEP        ( unsigned numBins  );
  unsigned          decodeRemAbsEP      ( unsigned goRicePar, unsigned cutoff, int maxLog2TrDynamicRange );
  unsigned          decodeBinTrm        ();
  unsigned          getNumBitsRead      () { return m_Bitstream->getNumBitsRead() + m_bitsNeeded; }
private:
  unsigned          decodeAlignedBinsEP ( unsigned numBins  );
protected:
  InputBitstream*   m_Bitstream;
  uint32_t          m_Range;
  uint32_t          m_Value;
  int32_t           m_bitsNeeded;
};


class BinDecoder : public BinDecoderBase
{
public:
  BinDecoder()
    : BinDecoderBase()
    , m_Ctx         ( static_cast<CtxStore&>( *this   ) )
  {}
  ~BinDecoder() {}
  unsigned decodeBin ( unsigned ctxId );
private:
  CtxStore& m_Ctx;
};


