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

/** \file     DecLib.cpp
    \brief    decoder wrapper class
*/

#include "DecLib.h"

#include "CommonLib/dtrace_next.h"
#include "CommonLib/dtrace_buffer.h"

#include "NALread.h"

#ifdef TRACE_ENABLE_ITT
std::vector<__itt_domain*> itt_domain_decInst;
__itt_domain* itt_domain_dec           = __itt_domain_create( "Decode" );
__itt_domain* itt_domain_prs           = __itt_domain_create( "Parse" );
__itt_domain* itt_domain_oth           = __itt_domain_create( "Other" );

__itt_string_handle* itt_handle_alf    = __itt_string_handle_create( "ALF_CTU" );
__itt_string_handle* itt_handle_presao = __itt_string_handle_create( "PreSAO_Line" );
__itt_string_handle* itt_handle_sao    = __itt_string_handle_create( "SAO_CTU" );
__itt_string_handle* itt_handle_lfl    = __itt_string_handle_create( "LFL_CTU" );
__itt_string_handle* itt_handle_intra  = __itt_string_handle_create( "Intra_CTU" );
__itt_string_handle* itt_handle_inter  = __itt_string_handle_create( "Inter_CTU" );
__itt_string_handle* itt_handle_mider  = __itt_string_handle_create( "MI-Der_CTU" );
__itt_string_handle* itt_handle_lfcl   = __itt_string_handle_create( "Prep_ClearLF" );
__itt_string_handle* itt_handle_ext    = __itt_string_handle_create( "Prep_ExtBrdr" );
__itt_string_handle* itt_handle_dmvr   = __itt_string_handle_create( "MI-DMVR" );
__itt_string_handle* itt_handle_rsp    = __itt_string_handle_create( "Reshape_CTU" );

__itt_string_handle* itt_handle_parse  = __itt_string_handle_create( "Parse_Slice" );

__itt_string_handle* itt_handle_start  = __itt_string_handle_create( "Start_Pic" );
__itt_string_handle* itt_handle_done   = __itt_string_handle_create( "Pic_Done" );
__itt_string_handle* itt_handle_finish = __itt_string_handle_create( "Finish_Pic" );

__itt_string_handle* itt_handle_schedTasks = __itt_string_handle_create( "Scheduling_Tasks" );
__itt_string_handle* itt_handle_waitTasks  = __itt_string_handle_create( "Wait_for_Dec_Tasks" );


// create global domain for DecLib
__itt_domain* itt_domain_glb    = __itt_domain_create ( "Global" );
// create a global counter
__itt_counter itt_frame_counter = __itt_counter_create( "FrameNumber", "Global" );

#define ITT_TASKSTART( d, t ) __itt_task_begin( ( d ), __itt_null, __itt_null, ( t ) )
#define ITT_TASKEND( d, t )   __itt_task_end  ( ( d ) )
#else
#define ITT_TASKSTART( d, t )
#define ITT_TASKEND( d, t )
#endif

//! \ingroup DecoderLib
//! \{

DecLib::DecLib()
{
#ifdef TRACE_ENABLE_ITT
  itt_domain_dec->flags = 1;
  itt_domain_prs->flags = 1;
  itt_domain_glb->flags = 1;
  itt_domain_oth->flags = 1;
#endif
}

void DecLib::create(int numDecThreads, int parserFrameDelay)
{
  // run constructor again to ensure all variables, especially in DecLibParser have been reset
  this->~DecLib();
  new( this ) DecLib;

  if( numDecThreads < 0 )
  {
    numDecThreads = std::thread::hardware_concurrency();
  }

  m_decodeThreadPool.reset( new NoMallocThreadPool( numDecThreads, "DecThread" ) );

  if( parserFrameDelay < 0 )
  {
    CHECK( numDecThreads < 0, "invalid number of threads" );
    parserFrameDelay = numDecThreads;
  }
  m_parseFrameDelay = parserFrameDelay;

  m_picListManager.create( m_parseFrameDelay, ( int ) m_decLibRecon.size() );
  m_decLibParser.create  ( m_decodeThreadPool.get(), m_parseFrameDelay, ( int ) m_decLibRecon.size(), numDecThreads );
    
  int id=0;
  for( auto &dec: m_decLibRecon )
  {
    dec.create( m_decodeThreadPool.get(), id++ );
  }

  std::stringstream cssCap;
  cssCap << "THREADS="     << numDecThreads << "; "
         << "PARSE_DELAY=" << parserFrameDelay << "; ";
#if ENABLE_SIMD_OPT
  cssCap << "SIMD=NONE";
#endif

  m_sDecoderCapabilities = cssCap.str();
  msg( INFO, "[%s]\n", m_sDecoderCapabilities.c_str() );

  DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
}

void DecLib::destroy()
{
  if( m_decodeThreadPool )
  {
    m_decodeThreadPool->shutdown( true );
    m_decodeThreadPool.reset();
  }

  m_decLibParser.destroy();
  for( auto &dec: m_decLibRecon )
  {
    dec.destroy();
  }

  m_picListManager.deleteBuffers();
}

#if JVET_P0288_PIC_OUTPUT
Picture* DecLib::decode( InputNALUnit& nalu, int* pSkipFrame, int iTargetLayer )
#else
Picture* DecLib::decode( InputNALUnit& nalu, int* pSkipFrame )
#endif
{
  Picture * pcParsedPic = nullptr;
  if( m_iMaxTemporalLayer >= 0 && nalu.m_temporalId > m_iMaxTemporalLayer )
  {
    pcParsedPic = nullptr;
  }
  else
  {
#if JVET_P0288_PIC_OUTPUT
    pcParsedPic = m_decLibParser.parse( nalu, pSkipFrame, iTargetLayer );
#else
    pcParsedPic = m_decLibParser.parse( nalu, pSkipFrame );
#endif
  }

  if( pcParsedPic )
  {
    this->decompressPicture( pcParsedPic );
  }

  if( m_decLibParser.getParseNewPicture() &&
      ( pcParsedPic || nalu.isSlice() || nalu.m_nalUnitType == NAL_UNIT_EOS ) )
  {
    Picture* outPic = getNextOutputPic( false );
    CHECK_WARN( m_checkMissingOutput && !outPic, "missing output picture" ); // we don't need this CHECK in flushPic(), because flushPic() is usually only called until the first nullptr is returned
    if( outPic )
    {
      m_checkMissingOutput = true;
    }
    return outPic;
  }

  return nullptr;
}

Picture* DecLib::flushPic()
{
  // at end of file, fill the decompression queue and decode pictures until we get one out
  while( Picture* pcParsedPic = m_decLibParser.getNextDecodablePicture() )
  {
    this->decompressPicture( pcParsedPic );

    if( Picture* outPic = getNextOutputPic( false ) )
    {
      return outPic;
    }
  }

  // first try to get a picture without waiting for the decoder
  if( Picture* outPic = getNextOutputPic( false ) )
  {
    return outPic;
  }

  // if no picture is done, actually block and wait
  if( Picture* outPic = getNextOutputPic( true ) )
  {
    return outPic;
  }

  // At the very end reset parser state
  InputNALUnit eosNAL;
  eosNAL.m_nalUnitType = NAL_UNIT_EOS;
  m_decLibParser.parse( eosNAL, nullptr );
  m_checkMissingOutput = false;

  return nullptr;
}

#if JVET_R0270
int DecLib::finishPicture( Picture* pcPic, MsgLevel msgl, bool associatedWithNewClvs )
#else
int DecLib::finishPicture( Picture* pcPic, MsgLevel msgl )
#endif
{
#ifdef TRACE_ENABLE_ITT
  // increment Framecounter
  __itt_counter_inc( itt_frame_counter );
#endif

  Slice*  pcSlice = pcPic->slices[0];
  if( pcPic->wasLost )
  {
    msg( msgl, "POC %4d TId: %1d LOST\n", pcPic->poc, pcSlice->getTLayer() );
    pcPic->reconstructed = true;
    return pcPic->poc;
  }

  ITT_TASKSTART( itt_domain_oth, itt_handle_finish );

  char c = ( pcSlice->isIntra() ? 'I' : pcSlice->isInterP() ? 'P' : 'B' );
  if( !pcPic->referenced )
  {
    c += 32;  // tolower
  }

  //-- For time output for each slice
  msg( msgl, "POC %4d LId: %2d TId: %1d ( %c-SLICE, QP%3d%s ) ", pcPic->poc, pcPic->layerId,
         pcSlice->getTLayer(),
         c,
         pcSlice->getSliceQp(),
         pcPic->longTerm ? " [LT]" : "     " );
  msg( msgl, "[DT %6.3f] ", pcPic->getProcessingTime() );

  for (int iRefList = 0; iRefList < 2; iRefList++)
  {
    msg( msgl, "[L%d ", iRefList);
    for (int iRefIndex = 0; iRefIndex < pcSlice->getNumRefIdx(RefPicList(iRefList)); iRefIndex++)
    {
      msg( msgl, "%d ", pcSlice->getRefPOC(RefPicList(iRefList), iRefIndex));
    }
    msg( msgl, "] ");
  }

  msg( msgl, "\n");

//  pcPic->neededForOutput = (pcSlice->getPicHeader()->getPicOutputFlag() ? true : false);
#if JVET_R0270
  if (associatedWithNewClvs && pcPic->neededForOutput)
  {
    if (!pcSlice->getPPS()->getMixedNaluTypesInPicFlag() && pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL)
    {
      pcPic->neededForOutput = false;
    }
    else if (pcSlice->getPPS()->getMixedNaluTypesInPicFlag())
    {
      bool isRaslPic = true;
      for (int i = 0; isRaslPic && i < pcPic->numSlices; i++)
      {
        if (!(pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RASL || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_RADL))
        {
          isRaslPic = false;
        }
      }
      if (isRaslPic)
      {
        pcPic->neededForOutput = false;
      }
    }
  }
#endif

  m_picListManager.applyDoneReferencePictureMarking();

#if JVET_Q0044_SLICE_IDX_WITH_SUBPICS
  m_maxDecSubPicIdx = 0;
  m_maxDecSliceAddrInSubPic = -1;
#endif

  if( m_parseFrameDelay > 0 )
  {
    checkPictureHashSEI( pcPic );
  }

  ITT_TASKEND( itt_domain_oth, itt_handle_finish );

  return pcSlice->getPOC();
}

void DecLib::checkPictureHashSEI( Picture* pcPic )
{
  if( !m_decodedPictureHashSEIEnabled )
  {
    return;
  }

  CHECK( !pcPic->reconstructed, "picture not reconstructed" );

  SEIMessages                  pictureHashes = getSeisByType( pcPic->SEIs, SEI::DECODED_PICTURE_HASH );
  const SEIDecodedPictureHash* hash          = pictureHashes.size() > 0 ? (SEIDecodedPictureHash*)pictureHashes.front() : nullptr;
  if( pictureHashes.empty() )
  {
    msg( WARNING, "Warning: missing decoded picture hash SEI message.\n" );
    return;
  }
  if( pictureHashes.size() > 1 )
  {
    msg( WARNING, "Warning: Got multiple decoded picture hash SEI messages. Using first." );
  }
  msg( INFO, "         " );
  m_numberOfChecksumErrorsDetected += calcAndPrintHashStatus( pcPic->getRecoBuf(), hash, pcPic->cs->sps->getBitDepths(), INFO );
  msg( INFO, "\n" );
}

Picture* DecLib::getNextOutputPic( bool bFlush )
{
  if( bFlush )
  {
    // wait for last pictures in bitstream
    for( auto & dec: m_decLibRecon )
    {
      Picture* donePic = dec.waitForPrevDecompressedPic();
      if( donePic )
      {
        finishPicture( donePic );
      }
    }
  }
  if( m_picListManager.getFrontPic() == nullptr )
  {
    return nullptr;
  }

  const SPS* activeSPS      = m_picListManager.getFrontPic()->cs->sps.get();
  const int  maxNrSublayers = activeSPS->getMaxTLayers();

  int numReorderPicsHighestTid;
  int maxDecPicBufferingHighestTid;
  if( m_iMaxTemporalLayer == -1 || m_iMaxTemporalLayer >= maxNrSublayers )
  {
    numReorderPicsHighestTid     = activeSPS->getNumReorderPics( maxNrSublayers - 1 );
    maxDecPicBufferingHighestTid = activeSPS->getMaxDecPicBuffering( maxNrSublayers - 1 );
  }
  else
  {
    numReorderPicsHighestTid     = activeSPS->getNumReorderPics( m_iMaxTemporalLayer );
    maxDecPicBufferingHighestTid = activeSPS->getMaxDecPicBuffering( m_iMaxTemporalLayer );
  }

  Picture* outPic = m_picListManager.getNextOutputPic( numReorderPicsHighestTid, maxDecPicBufferingHighestTid, bFlush );
  CHECK( outPic && outPic->done.isBlocked(), "next output-pic is not done yet." );
  return outPic;
}

void DecLib::decompressPicture( Picture* pcPic )
{
  DecLibRecon* decLibInstance = &m_decLibRecon.front();
  move_to_end( m_decLibRecon.begin(), m_decLibRecon );

  while( pcPic->wasLost )
  {
    Picture* donePic = decLibInstance->waitForPrevDecompressedPic();
    if( donePic )
    {
      finishPicture( donePic );
    }

    m_decLibParser.recreateLostPicture( pcPic );
    finishPicture( pcPic );

    pcPic = m_decLibParser.getNextDecodablePicture();
    if( !pcPic )
    {
      msg(WARNING, "a lost picture was filled in, but no following picture is available for decoding.");
      return;
    }
  }

  Picture* donePic = decLibInstance->waitForPrevDecompressedPic();

  decLibInstance->decompressPicture( pcPic );

  if( donePic )
  {
    finishPicture( donePic );
  }
}

#define SEI_REPETITION_CONSTRAINT_LIST_SIZE  21

//! \}
