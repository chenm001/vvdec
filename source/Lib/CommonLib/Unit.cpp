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

/** \file     Unit.cpp
 *  \brief    defines unit as a set of blocks and basic unit types (coding, prediction, transform)
 */

#include "Unit.h"

#include "Buffer.h"
#include "Picture.h"
#include "ChromaFormat.h"

#include "UnitTools.h"
#include "UnitPartitioner.h"

 // ---------------------------------------------------------------------------
 // block method definitions
 // ---------------------------------------------------------------------------

Position CompArea::chromaPos( const ChromaFormat chromaFormat ) const
{
  if (isLuma(compID))
  {
    uint32_t scaleX = getComponentScaleX(compID, chromaFormat);
    uint32_t scaleY = getComponentScaleY(compID, chromaFormat);

    return Position(x >> scaleX, y >> scaleY);
  }
  else
  {
    return *this;
  }
}

Size CompArea::lumaSize( const ChromaFormat chromaFormat ) const
{
  if( isChroma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Size( width << scaleX, height << scaleY );
  }
  else
  {
    return *this;
  }
}

Size CompArea::chromaSize( const ChromaFormat chromaFormat ) const
{
  if( isLuma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Size( width >> scaleX, height >> scaleY );
  }
  else
  {
    return *this;
  }
}

Position CompArea::lumaPos( const ChromaFormat chromaFormat ) const
{
  if( isChroma( compID ) )
  {
    uint32_t scaleX = getComponentScaleX( compID, chromaFormat );
    uint32_t scaleY = getComponentScaleY( compID, chromaFormat );

    return Position( x << scaleX, y << scaleY );
  }
  else
  {
    return *this;
  }
}

// ---------------------------------------------------------------------------
// unit method definitions
// ---------------------------------------------------------------------------

UnitArea::UnitArea(const ChromaFormat _chromaFormat, const Area &_area) : chromaFormat(_chromaFormat)
{
  const uint32_t numCh = getNumberValidComponents( chromaFormat );

  blocks.resize_noinit( numCh );

  if( !numCh ) return;

  blocks[0].compID       = COMPONENT_Y;
  blocks[0].x            = _area.x;
  blocks[0].y            = _area.y;
  blocks[0].width        = _area.width;
  blocks[0].height       = _area.height;

  if( numCh == 1 ) return;

  const int csx = getChannelTypeScaleX( CH_C, chromaFormat );
  const int csy = getChannelTypeScaleY( CH_C, chromaFormat );
  
  blocks[1].compID       = COMPONENT_Cb;
  blocks[2].compID       = COMPONENT_Cr;
  blocks[1].x            = blocks[2].x            = ( _area.x >> csx );
  blocks[1].y            = blocks[2].y            = ( _area.y >> csy );
  blocks[1].width        = blocks[2].width        = ( _area.width  >> csx );
  blocks[1].height       = blocks[2].height       = ( _area.height >> csy );
}

bool UnitArea::contains(const UnitArea& other) const
{
  bool any = false;

  if( blocks[0].valid() && other.blocks[0].valid() )
  {
    any = true;
    if( !blocks[0].contains( other.blocks[0] ) ) return false;
  }

  if( blocks[1].valid() && other.blocks[1].valid() )
  {
    any = true;
    if( !blocks[1].contains( other.blocks[1] ) ) return false;
  }

  if( blocks[2].valid() && other.blocks[2].valid() )
  {
    any = true;
    if( !blocks[2].contains( other.blocks[2] ) ) return false;
  }

  return any;
}

const UnitArea UnitArea::singleChan(const ChannelType chType) const
{
#if 1
  UnitArea ret(chromaFormat);

  for (const auto &blk : blocks)
  {
    if (toChannelType(blk.compID) == chType)
    {
      ret.blocks.push_back(blk);
    }
    else
    {
      ret.blocks.push_back(CompArea());
    }
  }
#else
  UnitArea ret = *this;

  for( auto &blk : ret.blocks )
  {
    if( toChannelType( blk.compID ) != chType )
    {
      new ( &blk ) CompArea();
    }
  }
#endif
  return ret;
}

// ---------------------------------------------------------------------------
// coding unit method definitions
// ---------------------------------------------------------------------------

void CodingUnit::minInit( const UnitArea &unit )
{
  static_cast<UnitArea &>( *this ) = unit;

  setBcwIdx   ( BCW_DEFAULT );
  intraDir[0] = DC_IDX;

  refIdx[0]   = refIdx[1] = -1;
}

// ---------------------------------------------------------------------------
// prediction unit method definitions
// ---------------------------------------------------------------------------

CodingUnit& CodingUnit::operator=( const MotionInfo& mi )
{
  setInterDir( mi.interDir );

  for( uint32_t i = 0; i < NUM_REF_PIC_LIST_01; i++ )
  {
    refIdx[i] = mi.refIdx[i];
    mv [i][0] = mi.mv    [i];
  }

  return *this;
}

const MotionInfo& CodingUnit::getMotionInfo() const
{
  return ctuData->motion[cs->inCtuPos( lumaPos(), CH_L )];
}

const MotionInfo& CodingUnit::getMotionInfo( const Position& pos ) const
{
  CHECKD( !Y().contains( pos ), "Trying to access motion info outsied of PU" );
  return ctuData->motion[cs->inCtuPos( pos, CH_L )];
}

MotionBuf CodingUnit::getMotionBuf()
{
  return MotionBuf( const_cast<MotionInfo*>( &getMotionInfo() ), cs->getLFPMapStride(), g_miScaling.scaleHor( lwidth() ), g_miScaling.scaleVer( lheight() ) );
}

CMotionBuf CodingUnit::getMotionBuf() const
{
  return CMotionBuf( &getMotionInfo(), cs->getLFPMapStride(), g_miScaling.scaleHor( lwidth() ), g_miScaling.scaleVer( lheight() ) );
}

// ---------------------------------------------------------------------------
// XUCache: unit allocation cache
// ---------------------------------------------------------------------------

std::shared_ptr<CUCache> ThreadSafeCUCache::getCuCache()
{
  std::unique_lock<std::mutex> l(m_mutex);
  for( auto & c: m_cuCaches )
  {
    // we know the cache instance is available, when there is only one shared_ptr reference (our own) to the element
    if( c.unique() )
    {
      return c;
    }
  }
  // no cache instance available -> create a new one
  m_cuCaches.push_back( std::make_shared<CUCache>() );
  return m_cuCaches.back();
}

std::shared_ptr<TUCache> ThreadSafeCUCache::getTuCache()
{
  std::unique_lock<std::mutex> l( m_mutex );
  for( auto & t : m_tuCaches )
  {
    // we know the cache instance is available, when there is only one shared_ptr reference (our own) to the element
    if( t.unique() )
    {
      return t;
    }
  }
  // no cache instance available -> create a new one
  m_tuCaches.push_back( std::make_shared<TUCache>() );
  return m_tuCaches.back();
}
