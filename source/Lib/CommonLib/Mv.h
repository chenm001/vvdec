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

/** \file     Mv.h
    \brief    motion vector class (header)
*/

#ifndef __MV__
#define __MV__

#include "CommonDef.h"

class SPS;
class PPS;
struct Position;
struct Size;

//! \ingroup CommonLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

enum MvPrecision
{
  MV_PRECISION_4PEL     = 0,      // 4-pel
  MV_PRECISION_INT      = 2,      // 1-pel, shift 2 bits from 4-pel
  MV_PRECISION_HALF     = 3,      // 1/2-pel
  MV_PRECISION_QUARTER  = 4,      // 1/4-pel (the precision of regular MV difference signaling), shift 4 bits from 4-pel
  MV_PRECISION_INTERNAL = 6,      // 1/16-pel (the precision of internal MV), shift 6 bits from 4-pel
};

/// basic motion vector class
struct Mv
{
#if defined( _DEBUG )
  static constexpr MvPrecision m_amvrPrecision[4] = { MV_PRECISION_QUARTER, MV_PRECISION_INT, MV_PRECISION_4PEL, MV_PRECISION_HALF };
#endif
  static constexpr uint16_t amvrPrecisionMagic = 0x614;
  static constexpr int mvClipPeriod = (1 << MV_BITS);
  static constexpr int halMvClipPeriod = (1 << (MV_BITS - 1));

  int32_t   hor;     ///< horizontal component of motion vector
  int32_t   ver;     ///< vertical component of motion vector

  // ------------------------------------------------------------------------------------------------------------------
  // constructors
  // ------------------------------------------------------------------------------------------------------------------

  constexpr Mv(                    ) : hor( 0    ), ver( 0    ) {}
  constexpr Mv( int iHor, int iVer ) : hor( iHor ), ver( iVer ) {}

  // ------------------------------------------------------------------------------------------------------------------
  // set
  // ------------------------------------------------------------------------------------------------------------------

  void  set       ( int iHor, int iVer)     { hor = iHor;  ver = iVer; }
  void  setZero   ()                        { hor = ver = 0;           }

  // ------------------------------------------------------------------------------------------------------------------
  // operations
  // ------------------------------------------------------------------------------------------------------------------

  const Mv& operator += ( const Mv& _rcMv )
  {
    hor += _rcMv.hor;
    ver += _rcMv.ver;

    return  *this;
  }

  const Mv& operator<<= (const int i)
  {
    hor <<= i;
    ver <<= i;
    return  *this;
  }

  const Mv operator - ( const Mv& rcMv ) const
  {
    return Mv( hor - rcMv.hor, ver - rcMv.ver );
  }

  const Mv operator + ( const Mv& rcMv ) const
  {
    return Mv( hor + rcMv.hor, ver + rcMv.ver );
  }

  bool operator== ( const Mv& rcMv ) const
  {
    return ( hor == rcMv.hor && ver == rcMv.ver );
  }

  bool operator!= ( const Mv& rcMv ) const
  {
    return !( *this == rcMv );
  }

  const Mv scaleMv( int iScale ) const
  {
    const int mvx = Clip3( -( 1 << 17 ), ( 1 << 17 ) - 1, (iScale * hor + 128 - (iScale * hor >= 0)) >> 8);
    const int mvy = Clip3( -( 1 << 17 ), ( 1 << 17 ) - 1, (iScale * ver + 128 - (iScale * ver >= 0)) >> 8);
    return Mv( mvx, mvy );
  }

  void changePrecision(const MvPrecision src, const MvPrecision& dst)
  {
    const int shift = (int)dst - (int)src;
    if (shift >= 0)
    {
      hor <<= shift;
      ver <<= shift;
    }
    else
    {
      const int rightShift = -shift;
      const int nOffset = 1 << (rightShift - 1);
      hor = hor >= 0 ? (hor + nOffset - 1) >> rightShift : (hor + nOffset) >> rightShift;
      ver = ver >= 0 ? (ver + nOffset - 1) >> rightShift : (ver + nOffset) >> rightShift;
    }
  }

  void changePrecisionAmvr(const int amvr, const MvPrecision& dst)
  {
    const int amvrPrecision = (amvrPrecisionMagic >> (amvr*3)) & 7;
    CHECKD(m_amvrPrecision[amvr] != amvrPrecision, "amvrPrecision check failed");
    changePrecision((const MvPrecision)amvrPrecision, dst);
  }

  void roundToPrecision(const MvPrecision& src, const MvPrecision& dst)
  {
    changePrecision(src, dst);
    changePrecision(dst, src);
  }

  void roundToAmvrSignalPrecision(const MvPrecision& src, const int amvr)
  {
    const int amvrPrecision = (amvrPrecisionMagic >> (amvr*3)) & 7;
    CHECKD(m_amvrPrecision[amvr] != amvrPrecision, "amvrPrecision check failed");
    roundToPrecision(src, (const MvPrecision)amvrPrecision);
  }

  void clipToStorageBitDepth()
  {
    hor = Clip3( -( 1 << 17 ), ( 1 << 17 ) - 1, hor );
    ver = Clip3( -( 1 << 17 ), ( 1 << 17 ) - 1, ver );
  }

  void mvCliptoStorageBitDepth()  // periodic clipping
  {
    hor = (hor + mvClipPeriod) & (mvClipPeriod - 1);
    hor = (hor >= halMvClipPeriod) ? (hor - mvClipPeriod) : hor;
    ver = (ver + mvClipPeriod) & (mvClipPeriod - 1);
    ver = (ver >= halMvClipPeriod) ? (ver - mvClipPeriod) : ver;
  }
};// END CLASS DEFINITION MV

#if JVET_R0058
extern void(*clipMv) ( Mv& rcMv, const Position& pos, const struct Size& size, const SPS& sps, const PPS& pps );
void clipMvInPic     ( Mv& rcMv, const Position& pos, const struct Size& size, const SPS& sps, const PPS& pps );
void clipMvInSubpic  ( Mv& rcMv, const Position& pos, const struct Size& size, const SPS& sps, const PPS& pps );
bool wrapClipMv      ( Mv& rcMv, const Position& pos, const struct Size& size, const SPS& sps, const PPS& pps );
#else
void clipMv    ( Mv& rcMv, const Position& pos,                   const SPS& sps, const PPS& pps, const int w = 0, const int h = 0 );
void clipMv    ( int& mvx, int& mvy, const Position& pos,                   const SPS& sps, const PPS& pps, const int w = 0, const int h = 0 );
bool wrapClipMv( Mv& rcMv, const Position& pos, const Size& size, const SPS& sps, const PPS& pps );
bool wrapClipMv( int& mvx, int& mvy, const Position& pos, const Size& size, const SPS& sps, const PPS& pps );
#endif

//#if JVET_O1164_PS
//bool wrapClipMv( Mv& rcMv, const Position& pos, const Size& size, const SPS& sps, const PPS& pps );
//bool wrapClipMv( int& mvx, int& mvy, const Position& pos, const Size& size, const SPS& sps, const PPS& pps );
//#else
//bool wrapClipMv( Mv& rcMv, const Position& pos, const Size& size, const SPS& sps );
//#endif

void roundAffineMv( int& mvx, int& mvy, int nShift );

//! \}

#endif // __MV__
