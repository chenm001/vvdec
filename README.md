# Fraunhofer Versatile Video Decoder (VVdeC)

Versatile Video Coding (VVC) is the most recent international video coding standard, developed by the Joint Video Experts Team (JVET) of the ITU-T Video Coding Experts Group (VCEG) and the ISO/IEC Moving Picture Experts Group (MPEG). VVC is the successor of the High Efficiency Video Coding (HEVC) standard and will be released by ITU-T as H.266 and by ISO/IEC as MPEG-I Part 3 (ISO/IEC 23090-3). The new standard targets a 50% bit-rate reduction over HEVC at the same visual quality. In addition, VVC proves to be truly versatile by including tools for efficient coding of video content in emerging applications, e.g. high dynamic range (HDR), adaptive streaming, computer generated content as well as immersive applications like 360 degree video and augmented reality (AR).

The Fraunhofer Versatile Video Decoder (VVdeC) is a fast VVC x86 software decoder implementation. The decoder supports most standard features available in the Main10 profile, with support for some high-level features still pending.

#  How to build VVdeC?

The software uses CMake to create platform-specific build files. 
A working CMake installation is required for building the software.
Download CMake from http://www.cmake.org/ and install it. The following targets are supported: Windows (Visual Studio), Linux (gcc) and MacOS (clang).

## Building using CMake

Open a command prompt on your system and change into the root directory of this project (location of this README.md file).

Create a build directory in the root directory:

    mkdir build

After that use one of the following cmake commands. Feel free to change the commands to satisfy your needs.

Windows sample for Visual Studio 2017 64 Bit:

    cd build
    cmake .. -G "Visual Studio 15 2017 Win64"

Linux Release Makefile sample:

    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release

Linux Debug Makefile sample:

    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug

MacOS-X Xcode sample:

    cd build
    cmake .. -G "Xcode"

Available CMake switches:
* VVDEC_ENABLE_BITSTREAM_DOWNLOAD: enables downloading of conformance bitstreams for testing
* VVDEC_ENABLE_INSTALL: enables creation of the install-target
    
## Building using plain make

The project includes an easy to use make interface which bundles the most important use-cases. 
    
Remarks:
* You still need to install CMake to use the make tool
* For Windows, you can install the make command as a part gnuwin32

Open a command prompt on your system and change into the root directory of this project (location of this README.md file).

To use the default system compiler simply call:

    make all

The project includes a simple test suite based on [JVET conformance bitstreams](https://www.itu.int/wftp3/av-arch/jvet-site/bitstream_exchange/VVC/). To enable it, call the make command with the following argument:

    make enable-bitstream-download=1 ...
    
To generate a solution for the default builder on your system simply call:

    make configure
    
To run the simple conformance test suite (if the bitstreams are downloaded and available) call:

    make test

# Contributing

Feel free to contribute. To do so:

* Fork the current-most state of the master branch
* Apply the desired changes
* Create a pull-request to the upstream repository

# __Chen's Roadmap__

- [ ] Performance @ 1080p (Thanks Xiaoquan You to report)

  Phone          | Fps
  :--------------|--------:
  Huawei Enjoy 9 | 11.60

- [x] Compatible  __209/238__

  Clips | Reason | Support Plan
  :------|:------:|:------:
  8b422_G_Sony_3 | Palette | No
  8b422_H_Sony_3 | Palette | No
  8b422_I_Sony_3 | Palette | No
  8b422_J_Sony_3 | Palette | No
  8b422_K_Sony_3 | Palette | No
  8b422_L_Sony_3 | Palette | No
  10b422_G_Sony_3 | Palette | No
  10b422_H_Sony_3 | Palette | No
  10b422_I_Sony_3 | Palette | No
  10b422_J_Sony_3 | Palette | No
  10b422_K_Sony_3 | Palette | No
  10b422_L_Sony_3 | Palette | No
  ACT_A_Kwai_2 | Sub-picture | No
  CodingToolsSets_E_Tencent_1 | Sub-picture | No
  FILLER_A_Bytedance_1 | NAL_UNIT_FD | TBD
  GDR_A_ERICSSON_2 | GDR | TBD
  GDR_A_NOKIA_1 | GDR | TBD
  GDR_B_NOKIA_1 | GDR | TBD
  GDR_C_NOKIA_1  | GDR | TBD
  LMCS_B_Dolby_2 | Sub-picture | No
  PALETTE_A_Alibaba_2 | Palette | No
  PALETTE_B_Alibaba_2 | Palette | No
  PALETTE_C_Alibaba_2 | Palette | No
  PALETTE_D_Alibaba_2 | Palette | No
  PALETTE_E_Alibaba_2 | Palette | No
  SUBPIC_A_HUAWEI_3 | Sub-picture | No
  SUBPIC_B_HUAWEI_3 | Sub-picture | No
  VIRTUAL_A_MediaTek_3 | Virtual | TBD
  VIRTUAL_B_MediaTek_3 | Virtual | TBD

- [x] Automatic verify framework (_test-harness_)

- [ ] Cleanup unnecessary C++ feature

- [x] Android r22 build

- [ ] Architecture improve (Chroma NV12, memory bandwidth optimize, etc)

- [ ] Partial Linker (Binary Code Protect)

- [ ] AArch64 Assembly

- [ ] Qualcomm HVX DSP

- [ ] Webassembly & js

- [ ] iOS build


# License

Please see [LICENSE.txt](./LICENSE.txt) file for the terms of use of the contents of this repository.

For more information, please contact: vvc@hhi.fraunhofer.de

**Copyright (c) 2018-2020 Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V.**

**All rights reserved.**
