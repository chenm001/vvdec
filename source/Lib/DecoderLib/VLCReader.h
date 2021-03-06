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

Copyright (c) 2018-2021, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 
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

/** \file     VLCWReader.h
 *  \brief    Reader for high level syntax
 */

#pragma once

#include "CommonLib/Rom.h"
#include "CommonLib/BitStream.h"
#include "CommonLib/Slice.h"
#include "CommonLib/SampleAdaptiveOffset.h"
#include "CABACReader.h"

#if ENABLE_TRACING

#define READ_SCODE(length, code, name)    xReadSCode  ( length, code, name )
#define READ_CODE(length, code, name)     xReadCodeTr ( length, code, name )
#define READ_UVLC(        code, name)     xReadUvlcTr (         code, name )
#define READ_SVLC(        code, name)     xReadSvlcTr (         code, name )
#define READ_FLAG(        code, name)     xReadFlagTr (         code, name )

#else


#define READ_SCODE(length, code, name)    xReadSCode ( length, code )
#define READ_CODE(length, code, name)     xReadCode ( length, code )
#define READ_UVLC(        code, name)     xReadUvlc (         code )
#define READ_SVLC(        code, name)     xReadSvlc (         code )
#define READ_FLAG(        code, name)     xReadFlag (         code )


#endif

namespace vvdec
{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

class VLCReader
{
protected:
  InputBitstream*   m_pcBitstream;

  VLCReader() : m_pcBitstream (NULL) {};
  virtual ~VLCReader() {};

  void  xReadCode    ( uint32_t   length, uint32_t& val );
  void  xReadUvlc    ( uint32_t& val );
  void  xReadSvlc    ( int& val );
  void  xReadFlag    ( uint32_t& val );
#if ENABLE_TRACING
  void  xReadCodeTr  ( uint32_t  length, uint32_t& rValue, const char *pSymbolName );
  void  xReadUvlcTr  ( uint32_t& rValue, const char *pSymbolName );
  void  xReadSvlcTr  ( int& rValue, const char *pSymbolName );
  void  xReadFlagTr  ( uint32_t& rValue, const char *pSymbolName );
#endif
#if ENABLE_TRACING
  void  xReadSCode   ( uint32_t length, int& val, const char *pSymbolName );
#else
  void  xReadSCode   ( uint32_t length, int& val );
#endif

public:
  void  setBitstream ( InputBitstream* p )   { m_pcBitstream = p; }
  InputBitstream* getBitstream() { return m_pcBitstream; }

protected:
  void xReadRbspTrailingBits();
  bool isByteAligned() { return ( m_pcBitstream->getNumBitsUntilByteAligned() == 0 ); }
};



class AUDReader: public VLCReader
{
public:
  AUDReader() {};
  virtual ~AUDReader() {};
  void parseAccessUnitDelimiter( InputBitstream* bs, uint32_t &picType );
};



class FDReader: public VLCReader
{
public:
  FDReader() {};
  virtual ~FDReader() {};
  void parseFillerData(InputBitstream* bs, uint32_t &fdSize);
};



class HLSyntaxReader : public VLCReader
{
public:
  HLSyntaxReader()          = default;
  virtual ~HLSyntaxReader() = default;

protected:
  void  copyRefPicList      ( SPS* pcSPS, ReferencePictureList* source_rpl, ReferencePictureList* dest_rpl );
  void  parseRefPicList     (const SPS* pcSPS, ReferencePictureList* rpl, int rplIdx );

public:
  void  parseVPS            ( VPS* pcVPS );
  void  parseDCI            ( DCI* dci );
  void  parseSPS            ( SPS* pcSPS, ParameterSetManager *parameterSetManager );
  void  parsePPS            ( PPS* pcPPS, ParameterSetManager *parameterSetManager );
  void  parseAPS            ( APS* pcAPS);
  void  parseAlfAps         ( APS* pcAPS );
  void  parseLmcsAps        ( APS* pcAPS );
  void  parseScalingListAps ( APS* pcAPS );
  void  parseVUI            ( VUI* pcVUI, SPS* pcSPS );
  void  parseConstraintInfo ( ConstraintInfo *cinfo );
#if JVET_Q0786_PTL_only
  void  parseProfileTierLevel( ProfileTierLevel *ptl, bool profileTierPresentFlag, int maxNumSubLayersMinus1 );
#else
  void  parseProfileTierLevel( ProfileTierLevel *ptl, int maxNumSubLayersMinus1 );
#endif
  void  parseOlsHrdParameters    ( GeneralHrdParams* generalHrd, OlsHrdParams *olsHrd, uint32_t firstSubLayer, uint32_t tempLevelHigh );
  void  parseGeneralHrdParameters( GeneralHrdParams *generalHrd );
  void  parsePictureHeader  ( PicHeader* picHeader, ParameterSetManager *parameterSetManager, bool readRbspTrailingBits );
  void  parseSliceHeader    ( Slice* pcSlice, PicHeader* parsedPicHeader, ParameterSetManager *parameterSetManager, const int prevTid0POC, Picture* parsePic , bool& firstSliceInPic );
  template<typename HeaderT>
  void  parsePicOrSliceHeaderRPL( HeaderT* header, const SPS* sps, const PPS* pps );
  void  checkAlfNaluTidAndPicTid( Slice* pcSlice, PicHeader* picHeader, ParameterSetManager *parameterSetManager);
  void  getSlicePoc         ( Slice* pcSlice, PicHeader* picHeader, ParameterSetManager *parameterSetManager, const int prevTid0POC );
  void  parseTerminatingBit ( uint32_t& ruiBit );
  void  parseRemainingBytes ( bool noTrailingBytesExpected );

  void  parsePredWeightTable( Slice* pcSlice, const SPS *sps );
  void  parsePredWeightTable( PicHeader *picHeader, const SPS *sps );
  void  parseScalingList    ( ScalingList *scalingList, bool aps_chromaPresentFlag );
  void  decodeScalingList   ( ScalingList *scalingList, uint32_t scalingListId, bool isPredictor );
  void  parseReshaper       ( SliceReshapeInfo& sliceReshaperInfo, const SPS* pcSPS, const bool isIntra );
  void  alfFilter           ( AlfSliceParam& alfSliceParam, const bool isChroma, const int altIdx );
  void  ccAlfFilter         ( Slice *pcSlice );
#if JVET_P0117_PTL_SCALABILITY
  void  dpb_parameters      ( int maxSubLayersMinus1, bool subLayerInfoFlag, SPS *pcSPS );
#endif
  void  parseExtraPHBitsStruct( SPS *sps, int numBytes );
  void  parseExtraSHBitsStruct( SPS *sps, int numBytes );
private:

protected:
  bool  xMoreRbspData();
};

}
