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

/*
 * \ingroup CommonLib
 * \file    InitX86.cpp
 * \brief   Initialize encoder SIMD functions.
 */


#include "CommonLib/CommonDef.h"
#include "CommonLib/InterpolationFilter.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/RdCost.h"
#include "CommonLib/Buffer.h"
#include "CommonLib/TrQuant_EMT.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/LoopFilter.h"
#include "CommonLib/Picture.h"

#include "CommonLib/AdaptiveLoopFilter.h"
#include "CommonLib/SampleAdaptiveOffset.h"

namespace vvdec
{

#ifdef TARGET_SIMD_X86


#if ENABLE_SIMD_OPT_MCIF
void InterpolationFilter::initInterpolationFilterX86( /*int iBitDepthY, int iBitDepthC*/ )
{
  auto vext = read_x86_extension_flags();
  switch (vext){
  case AVX512:
  case AVX2:
    _initInterpolationFilterX86<AVX2>(/*iBitDepthY, iBitDepthC*/);
    break;
  case AVX:
    _initInterpolationFilterX86<AVX>(/*iBitDepthY, iBitDepthC*/);
    break;
  case SSE42:
  case SSE41:
    _initInterpolationFilterX86<SSE41>(/*iBitDepthY, iBitDepthC*/);
    break;
  default:
    break;
  }
}
#endif

#if ENABLE_SIMD_OPT_BUFFER
void PelBufferOps::initPelBufOpsX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initPelBufOpsX86<AVX2>();
      break;
    case AVX:
      _initPelBufOpsX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initPelBufOpsX86<SSE41>();
      break;
    default:
      break;
  }
}
#endif




#if ENABLE_SIMD_OPT_DIST
void RdCost::initRdCostX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initRdCostX86<AVX2>();
      break;
    case AVX:
      _initRdCostX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initRdCostX86<SSE41>();
      break;
    default:
      break;
  }
}
#endif

#if ENABLE_SIMD_OPT_ALF
void AdaptiveLoopFilter::initAdaptiveLoopFilterX86()
{
  auto vext = read_x86_extension_flags();
  switch ( vext )
  {
  case AVX512:
  case AVX2:
    _initAdaptiveLoopFilterX86<AVX2>();
    break;
  case AVX:
    _initAdaptiveLoopFilterX86<AVX>();
    break;
  case SSE42:
  case SSE41:
    _initAdaptiveLoopFilterX86<SSE41>();
    break;
  default:
    break;
  }
}
#endif

#if ENABLE_SIMD_DBLF
void LoopFilter::initLoopFilterX86()
{
  auto vext = read_x86_extension_flags();
  switch ( vext )
  {
  case AVX512:
  case AVX2:
    _initLoopFilterX86<AVX2>();
    break;
  case AVX:
    _initLoopFilterX86<AVX>();
    break;
  case SSE42:
  case SSE41:
    _initLoopFilterX86<SSE41>();
    break;
  default:
    break;
  }
}
#endif

#if ENABLE_SIMD_TCOEFF_OPS
void TCoeffOps::initTCoeffOpsX86()
{
  auto vext = read_x86_extension_flags();

  switch( vext )
  {
  case AVX512:
  case AVX2:
    _initTCoeffOpsX86<AVX2>();
    break;
  case AVX:
    _initTCoeffOpsX86<AVX>();
    break;
  case SSE42:
  case SSE41:
    _initTCoeffOpsX86<SSE41>();
    break;
  default:
  break;
  }
}
#endif

#if ENABLE_SIMD_OPT_INTRAPRED
void IntraPrediction::initIntraPredictionX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initIntraPredictionX86<AVX2>();
      break;
    case AVX:
      _initIntraPredictionX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initIntraPredictionX86<SSE41>();
      break;
    default:
      break;
  }

}

#endif
#if ENABLE_SIMD_OPT_SAO
void SampleAdaptiveOffset::initSampleAdaptiveOffsetX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initSampleAdaptiveOffsetX86<AVX2>();
      break;
    case AVX:
      _initSampleAdaptiveOffsetX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initSampleAdaptiveOffsetX86<SSE41>();
      break;
    default:
      break;
  }

}

#endif

#if ENABLE_SIMD_OPT_BIO
void InterPrediction::initInterPredictionX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initInterPredictionX86<AVX2>();
      break;
    case AVX:
      _initInterPredictionX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initInterPredictionX86<SSE41>();
      break;
    default:
      break;
  }

}
#endif

#if ENABLE_SIMD_OPT_PICTURE
void Picture::initPictureX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initPictureX86<AVX2>();
      break;
    case AVX:
      _initPictureX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initPictureX86<SSE41>();
      break;
    default:
      break;
  }

}
#endif

#if ENABLE_SIMD_OPT_QUANT
void Quant::initQuantX86()
{
  auto vext = read_x86_extension_flags();
  switch (vext){
    case AVX512:
    case AVX2:
      _initQuantX86<AVX2>();
      break;
    case AVX:
      _initQuantX86<AVX>();
      break;
    case SSE42:
    case SSE41:
      _initQuantX86<SSE41>();
      break;
    default:
      break;
  }

}

#endif



#endif

}