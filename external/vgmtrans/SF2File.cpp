/*
* VGMTrans (c) 2002-2024
 * Licensed under the zlib license,
 * refer to the included LICENSE.txt file
 */
#include <cmath>
#include <iostream>
#include "version.h"
#include "ScaleConversion.h"
#include "SF2File.h"
//#include "Root.h"

#include "rsnd/SoundBank.hpp"
#include "rsnd/SoundWaveArchive.hpp"
#include "rsnd/SoundWave.hpp"
#include "common/fileUtil.hpp"

struct EnvelopeParams {
  double attack_time;
  double decay_time;
  double sustain_level;
  double release_time;
  double hold_time;
};

static float GetFallingRate(uint8_t DecayTime) {
  if (DecayTime == 0x7F)
    return 65535.0f;
  else if (DecayTime == 0x7E)
    return 120 / 5.0f;
  else if (DecayTime < 0x32)
    return ((DecayTime << 1) + 1) / 128.0f / 5.0f;
  else
    return ((60.0f) / (126 - DecayTime)) / 5.0f;
}

// taken from https://github.com/soneek/3DSUSoundArchiveTool
static double LogB( double n, double b ) {
  // log(n)/log(2) is log2.
  return log( n ) / log( b );
}

// taken from https://github.com/soneek/3DSUSoundArchiveTool
static double timeToTimecents(double time){
  double timeCent = floor(1200 * LogB(time, 2));
  if (timeCent > -12000 && timeCent < 0) {
    return timeCent + 65536;
  } else if (timeCent < -12000) {
    return -12000;
  } else {
    return timeCent;
  }
}

static EnvelopeParams envelopeFromInfo(const rsnd::InstrInfo* info) {
  // mostly taken from https://github.com/soneek/3DSUSoundArchiveTool
  EnvelopeParams envelope;

  double attackTable[] = {13122, 6546, 4356, 3261, 2604, 2163, 1851, 1617, 1434, 1287, 1167, 1068, 984, 912, 849, 795, 747, 702, 666, 630, 600, 570, 543, 519, 498, 477, 459, 441, 426, 411, 396, 384, 372, 360, 348, 336, 327, 318, 309, 300, 294, 285, 279, 270, 264, 258, 252, 246, 240, 234, 231, 225, 219, 216, 210, 207, 201, 198, 195, 192, 186, 183, 180, 177, 174, 171, 168, 165, 162, 159, 156, 153.5, 153, 150, 147, 144, 141.5, 141, 138, 135.5, 135, 132, 129.5, 129, 126, 123.5, 123, 120.5, 120, 117, 114.5, 114, 111.5, 111, 108.5, 108, 105.7, 105.35, 105, 102.5, 102, 99.5, 99, 96.7, 96.35, 96, 93.5, 93, 90, 87, 81, 75, 72, 69, 63, 60, 54, 48, 45, 39, 36, 30, 24, 21, 15, 12, 9, 6.1e-6};
  double holdTable[] = {6e-6, 1, 2, 4, 6, 9, 12, 16, 20, 25, 30, 36, 42, 49, 56, 64, 72, 81, 90, 100, 110, 121, 132, 144, 156, 169, 182, 196, 210, 225, 240, 256, 272, 289, 306, 324, 342, 361, 380, 400, 420, 441, 462, 484, 506, 529, 552, 576, 600, 625, 650, 676, 702, 729, 756, 784, 812, 841, 870, 900, 930, 961, 992, 1024, 1056, 1089, 1122, 1156, 1190, 1225, 1260, 1296, 1332, 1369, 1406, 1444, 1482, 1521, 1560, 1600, 1640, 1681, 1722, 1764, 1806, 1849, 1892, 1936, 1980, 2025, 2070, 2116, 2162, 2209, 2256, 2304, 2352, 2401, 2450, 2500, 2550, 2601, 2652, 2704, 2756, 2809, 2862, 2916, 2970, 3025, 3080, 3136, 3192, 3249, 3306, 3364, 3422, 3481, 3540, 3600, 3660, 3721, 3782, 3844, 3906, 3969, 4032, 4096};
  double decayTable[] = { -0.00016, -0.00047, -0.00078, -0.00109, -0.00141, -0.00172, -0.00203, -0.00234, -0.00266, -0.00297, -0.00328, -0.00359, -0.00391, -0.00422, -0.00453, -0.00484, -0.00516, -0.00547, -0.00578, -0.00609, -0.00641, -0.00672, -0.00703, -0.00734, -0.00766, -0.00797, -0.00828, -0.00859, -0.00891, -0.00922, -0.00953, -0.00984, -0.01016, -0.01047, -0.01078, -0.01109, -0.01141, -0.01172, -0.01203, -0.01234, -0.01266, -0.01297, -0.01328, -0.01359, -0.01391, -0.01422, -0.01453, -0.01484, -0.01516, -0.01547, -0.01579, -0.016, -0.01622, -0.01644, -0.01667, -0.0169, -0.01714, -0.01739, -0.01765, -0.01791, -0.01818, -0.01846, -0.01875, -0.01905, -0.01935, -0.01967, -0.02, -0.02034, -0.02069, -0.02105, -0.02143, -0.02182, -0.02222, -0.02264, -0.02308, -0.02353, -0.024, -0.02449, -0.025, -0.02553, -0.02609, -0.02667, -0.02727, -0.02791, -0.02857, -0.02927, -0.03, -0.03077, -0.03158, -0.03243, -0.03333, -0.03429, -0.03529, -0.03636, -0.0375, -0.03871, -0.04, -0.04138, -0.04286, -0.04444, -0.04615, -0.048, -0.05, -0.05217, -0.05455, -0.05714, -0.06, -0.06316, -0.06667, -0.07059, -0.075, -0.08, -0.08571, -0.09231, -1, -0.10909, -0.12, -0.13333, -0.15, -0.17143, -2, -2.4, -3, -4, -6, -12, -24, -65535 };

  envelope.attack_time = timeToTimecents(attackTable[info->attack] / 1000);

  double sustainVol = 20 * LogB(pow(((double)info->sustain / 127.0000), 2), 10);

  if (info->decay == 127) {
    envelope.decay_time = -12000;
  } else {
    if (info->sustain == 0) {
      envelope.decay_time = timeToTimecents(-90.25 / decayTable[info->decay] / 1000);
    } else {
      envelope.decay_time = timeToTimecents(sustainVol / decayTable[info->decay] / 1000);
    }
  }

  if (info->sustain == 0) {
    envelope.sustain_level = 900;
  } else {
    envelope.sustain_level = 200 * abs(LogB(pow(((double)info->sustain / 127.0000), 2), 10));
  }

  if (info->release == 127) {
    envelope.release_time = -12000;
  } else {
    if (info->sustain == 0) {
			envelope.release_time = timeToTimecents(-90.25 / decayTable[info->release] / 1000);
    } else {
			envelope.release_time = timeToTimecents((-90.25 - sustainVol) / decayTable[info->release] / 1000);
    }
  }

  envelope.hold_time = timeToTimecents(holdTable[info->hold] / 1000);

  return envelope;
}

SF2InfoListChunk::SF2InfoListChunk(const std::string& name)
    : LISTChunk("INFO") {
  // Create a date string
  time_t current_time = time(nullptr);
  char *c_time_string = ctime(&current_time);

  // Add the child info chunks
  Chunk *ifilCk = new Chunk("ifil");
  sfVersionTag versionTag;        //soundfont version 2.01
  versionTag.wMajor = 2;
  versionTag.wMinor = 1;
  ifilCk->SetData(&versionTag, sizeof(versionTag));
  AddChildChunk(ifilCk);
  AddChildChunk(new SF2StringChunk("isng", "EMU8000"));
  AddChildChunk(new SF2StringChunk("INAM", name));
  AddChildChunk(new SF2StringChunk("ICRD", std::string(c_time_string)));
  AddChildChunk(new SF2StringChunk("ISFT", std::string("VGMTrans " + std::string(VGMTRANS_VERSION))));
}


//  *******
//  SF2File
//  *******

SF2File::SF2File(const rsnd::SoundBank *bankfile, const std::vector<WaveAudio>& waves)
    : RiffFile("RSND bank", "sfbk") {

  //***********
  // INFO chunk
  //***********
  AddChildChunk(new SF2InfoListChunk(name));

  // sdta chunk and its child smpl chunk containing all samples
  LISTChunk *sdtaCk = new LISTChunk("sdta");
  Chunk *smplCk = new Chunk("smpl");

  // Concatanate all of the samples together and add the result to the smpl chunk data
  size_t numWaves = waves.size();
  smplCk->size = 0;
  for (size_t i = 0; i < numWaves; i++) {
    const WaveAudio& wav = waves[i];
    smplCk->size += wav.dataLength + (46 * 2);    // plus the 46 padding samples required by sf2 spec
  }
  smplCk->data = new uint8_t[smplCk->size];
  uint32_t bufPtr = 0;
  for (size_t i = 0; i < numWaves; i++) {
    const WaveAudio& wav = waves[i];
    size_t waveSz = wav.dataLength;

    void* pcm = wav.data;
    memcpy(smplCk->data + bufPtr, pcm, waveSz);
    memset(smplCk->data + bufPtr + waveSz, 0, 46 * 2);
    bufPtr += waveSz + (46 * 2);        // plus the 46 padding samples required by sf2 spec
  }

  sdtaCk->AddChildChunk(smplCk);
  this->AddChildChunk(sdtaCk);

  //***********
  // pdta chunk
  //***********

  LISTChunk *pdtaCk = new LISTChunk("pdta");

  //***********
  // phdr chunk
  //***********
  Chunk *phdrCk = new Chunk("phdr");
  size_t numInstrs = bankfile->getInstrCount();
  phdrCk->size = static_cast<uint32_t>((numInstrs + 1) * sizeof(sfPresetHeader));
  phdrCk->data = new uint8_t[phdrCk->size];

  for (size_t i = 0; i < numInstrs; i++) {
    std::string instrName = "instr" + std::to_string(i);
    int wPreset = i;
    // I don't think these are known for RBNK files, so set to zero
    int wBank = 0;

    sfPresetHeader presetHdr{};
    memcpy(presetHdr.achPresetName, instrName.c_str(), std::min(instrName.length(), static_cast<size_t>(20)));
    presetHdr.wPreset = static_cast<uint16_t>(wPreset);

    // Despite being a 16-bit value, SF2 only supports banks up to 127. Since
    // it's pretty common to have either MSB or LSB be 0, we'll use whatever
    // one is not zero, with preference for MSB.
    //uint16_t bank16 = static_cast<uint16_t>(instr->ulBank);

    if ((wBank & 0xFF00) == 0) {
      presetHdr.wBank = wBank & 0x7F;
    }
    else {
      presetHdr.wBank = (wBank >> 8) & 0x7F;
    }
    presetHdr.wPresetBagNdx = static_cast<uint16_t>(i);
    presetHdr.dwLibrary = 0;
    presetHdr.dwGenre = 0;
    presetHdr.dwMorphology = 0;

    memcpy(phdrCk->data + (i * sizeof(sfPresetHeader)), &presetHdr, sizeof(sfPresetHeader));
  }
  //  add terminal sfPresetBag
  sfPresetHeader presetHdr{};
  presetHdr.wPresetBagNdx = static_cast<uint16_t>(numInstrs);
  memcpy(phdrCk->data + (numInstrs * sizeof(sfPresetHeader)), &presetHdr, sizeof(sfPresetHeader));
  pdtaCk->AddChildChunk(phdrCk);

  //***********
  // pbag chunk
  //***********
  Chunk *pbagCk = new Chunk("pbag");
  constexpr size_t ITEMS_IN_PGEN = 2;
  pbagCk->size = static_cast<uint32_t>((numInstrs + 1) * sizeof(sfPresetBag));
  pbagCk->data = new uint8_t[pbagCk->size];
  for (size_t i = 0; i < numInstrs; i++) {
    sfPresetBag presetBag{};
    presetBag.wGenNdx = static_cast<uint16_t>(i * ITEMS_IN_PGEN);
    presetBag.wModNdx = 0;

    memcpy(pbagCk->data + (i * sizeof(sfPresetBag)), &presetBag, sizeof(sfPresetBag));
  }
  //  add terminal sfPresetBag
  sfPresetBag presetBag{};
  presetBag.wGenNdx = static_cast<uint16_t>(numInstrs * ITEMS_IN_PGEN);
  memcpy(pbagCk->data + (numInstrs * sizeof(sfPresetBag)), &presetBag, sizeof(sfPresetBag));
  pdtaCk->AddChildChunk(pbagCk);

  //***********
  // pmod chunk
  //***********
  Chunk *pmodCk = new Chunk("pmod");
  //  create the terminal field
  sfModList modList{};
  pmodCk->SetData(&modList, sizeof(sfModList));
  //modList.sfModSrcOper = cc1_Mod;
  //modList.sfModDestOper = startAddrsOffset;
  //modList.modAmount = 0;
  //modList.sfModAmtSrcOper = cc1_Mod;
  //modList.sfModTransOper = linear;
  pdtaCk->AddChildChunk(pmodCk);

  //***********
  // pgen chunk
  //***********
  Chunk *pgenCk = new Chunk("pgen");
  //pgenCk->size = (synthfile->vInstrs.size()+1) * sizeof(sfGenList);
  pgenCk->size = static_cast<uint32_t>((numInstrs * sizeof(sfGenList) * ITEMS_IN_PGEN) + sizeof(sfGenList));
  pgenCk->data = new uint8_t[pgenCk->size];
  uint32_t dataPtr = 0;
  for (size_t i = 0; i < numInstrs; i++) {
    // VGMTrans supports reverb, not used by BRBNK. Perhaps these entries can be omitted
    int instrReverb = 0;

    sfGenList genList{};

    // reverbEffectsSend - Value is in 0.1% units, so multiply by 1000. Ex: 250 == 25%.
    genList.sfGenOper = reverbEffectsSend;
    genList.genAmount.shAmount = instrReverb * 1000;
    memcpy(pgenCk->data + dataPtr, &genList, sizeof(sfGenList));
    dataPtr += sizeof(sfGenList);

    genList.sfGenOper = instrument;
    genList.genAmount.wAmount = static_cast<uint16_t>(i);
    memcpy(pgenCk->data + dataPtr, &genList, sizeof(sfGenList));
    dataPtr += sizeof(sfGenList);
  }
  //  add terminal sfGenList
  sfGenList genList{};
  memcpy(pgenCk->data + dataPtr, &genList, sizeof(sfGenList));

  pdtaCk->AddChildChunk(pgenCk);

  //***********
  // inst chunk
  //***********
  Chunk *instCk = new Chunk("inst");
  instCk->size = static_cast<uint32_t>((numInstrs + 1) * sizeof(sfInst));
  instCk->data = new uint8_t[instCk->size];
  uint16_t rgnCounter = 0;
  for (size_t i = 0; i < numInstrs; i++) {
    std::string instrName = "instr" + std::to_string(i);
    auto regions = bankfile->getInstrRegions(i);

    sfInst inst{};
    memcpy(inst.achInstName, instrName.c_str(), std::min(instrName.length(), static_cast<size_t>(20)));
    inst.wInstBagNdx = rgnCounter;
    rgnCounter += regions.size();

    memcpy(instCk->data + (i * sizeof(sfInst)), &inst, sizeof(sfInst));
  }
  //  add terminal sfInst
  uint32_t numTotalRgns = rgnCounter;
  sfInst inst{};
  inst.wInstBagNdx = numTotalRgns;
  memcpy(instCk->data + (numInstrs * sizeof(sfInst)), &inst, sizeof(sfInst));
  pdtaCk->AddChildChunk(instCk);

  //***********
  // ibag chunk - stores all zones (regions) for instruments
  //***********
  Chunk *ibagCk = new Chunk("ibag");


  ibagCk->size = (numTotalRgns + 1) * sizeof(sfInstBag);
  ibagCk->data = new uint8_t[ibagCk->size];

  rgnCounter = 0;
  int instGenCounter = 0;
  for (size_t i = 0; i < numInstrs; i++) {
    auto regions = bankfile->getInstrRegions(i);

    size_t numRgns = regions.size();
    for (size_t j = 0; j < numRgns; j++) {
      sfInstBag instBag{};
      instBag.wInstGenNdx = instGenCounter;
      instGenCounter += 12;
      instBag.wInstModNdx = 0;

      memcpy(ibagCk->data + (rgnCounter++ * sizeof(sfInstBag)), &instBag, sizeof(sfInstBag));
    }
  }
  //  add terminal sfInstBag
  sfInstBag instBag{};
  instBag.wInstGenNdx = instGenCounter;
  instBag.wInstModNdx = 0;
  memcpy(ibagCk->data + (rgnCounter * sizeof(sfInstBag)), &instBag, sizeof(sfInstBag));
  pdtaCk->AddChildChunk(ibagCk);

  //***********
  // imod chunk
  //***********
  Chunk *imodCk = new Chunk("imod");
  //  create the terminal field
  memset(&modList, 0, sizeof(sfModList));
  imodCk->SetData(&modList, sizeof(sfModList));
  pdtaCk->AddChildChunk(imodCk);

  //***********
  // igen chunk
  //***********
  Chunk *igenCk = new Chunk("igen");
  igenCk->size = (numTotalRgns * sizeof(sfInstGenList) * 12) + sizeof(sfInstGenList);
  igenCk->data = new uint8_t[igenCk->size];
  dataPtr = 0;
  for (size_t i = 0; i < numInstrs; i++) {
    auto instrRegions = bankfile->getInstrRegions(i);

    size_t numRgns = instrRegions.size();
    for (size_t j = 0; j < numRgns; j++) {
      //SynthRgn *rgn = instr->vRgns[j];
      auto instrRegion = instrRegions[j];

      sfInstGenList instGenList;
      // Key range - (if exists) this must be the first chunk
      instGenList.sfGenOper = keyRange;
      instGenList.genAmount.ranges.byLo = static_cast<uint8_t>(instrRegion.keyLo);
      instGenList.genAmount.ranges.byHi = static_cast<uint8_t>(instrRegion.keyHi);
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      if (instrRegion.velHi)    // 0 means 'not set', fixes TriAce instruments
      {
        // Velocity range (if exists) this must be the next chunk
        instGenList.sfGenOper = velRange;
        instGenList.genAmount.ranges.byLo = static_cast<uint8_t>(instrRegion.velLo);
        instGenList.genAmount.ranges.byHi = static_cast<uint8_t>(instrRegion.velHi);
        memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
        dataPtr += sizeof(sfInstGenList);
      }

      auto* instrInfo = instrRegion.instrInfo;
      EnvelopeParams envelope = envelopeFromInfo(instrInfo);
      const WaveAudio& wav = waves[instrInfo->waveIdx];

      // initialAttenuation
      instGenList.sfGenOper = initialAttenuation;
      instGenList.genAmount.shAmount = 127 - instrInfo->volume; // TODO: how do I transcribe volume?
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // pan
      instGenList.sfGenOper = pan;
      int16_t panConverted = std::round(1000 * (instrInfo->pan - 64) / 64.0);
      instGenList.genAmount.shAmount = panConverted;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // sampleModes
      instGenList.sfGenOper = sampleModes;
      instGenList.genAmount.wAmount = wav.loop;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // overridingRootKey
      instGenList.sfGenOper = overridingRootKey;
      instGenList.genAmount.wAmount = instrInfo->originalKey;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // attackVolEnv
      instGenList.sfGenOper = attackVolEnv;
      instGenList.genAmount.shAmount = envelope.attack_time;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // holdVolEnv
      instGenList.sfGenOper = holdVolEnv;
      instGenList.genAmount.shAmount = envelope.hold_time;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // decayVolEnv
      instGenList.sfGenOper = decayVolEnv;
      instGenList.genAmount.shAmount = envelope.decay_time;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // sustainVolEnv
      instGenList.sfGenOper = sustainVolEnv;
      instGenList.genAmount.shAmount = static_cast<int16_t>(envelope.sustain_level);
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // releaseVolEnv
      instGenList.sfGenOper = releaseVolEnv;
      instGenList.genAmount.shAmount = envelope.release_time;
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      // reverbEffectsSend
      //instGenList.sfGenOper = reverbEffectsSend;
      //instGenList.genAmount.shAmount = 800;
      //memcpy(pgenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      //dataPtr += sizeof(sfInstGenList);

      // sampleID - this is the terminal chunk
      instGenList.sfGenOper = sampleID;
      instGenList.genAmount.wAmount = static_cast<uint16_t>(instrInfo->waveIdx);
      memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
      dataPtr += sizeof(sfInstGenList);

      //int numConnBlocks = rgn->art->vConnBlocks.size();
      //for (int k = 0; k < numConnBlocks; k++)
      //{
      //	SynthConnectionBlock* connBlock = rgn->art->vConnBlocks[k];
      //	connBlock->
      //}
    }
  }
  //  add terminal sfInstBag
  sfInstGenList instGenList{};
  memcpy(igenCk->data + dataPtr, &instGenList, sizeof(sfInstGenList));
  //memset(ibagCk->data + (numRgns*sizeof(sfInstBag)), 0, sizeof(sfInstBag));
  //igenCk->SetData(&genList, sizeof(sfGenList));
  pdtaCk->AddChildChunk(igenCk);

  //***********
  // shdr chunk
  //***********
  Chunk *shdrCk = new Chunk("shdr");

  size_t numSamps = waves.size();
  shdrCk->size = static_cast<uint32_t>((numSamps + 1) * sizeof(sfSample));
  shdrCk->data = new uint8_t[shdrCk->size];

  uint32_t sampOffset = 0;
  for (size_t i = 0; i < numSamps; i++) {
    const WaveAudio& wav = waves[i];
    std::string waveName = "wav" + std::to_string(i);

    sfSample samp{};
    memcpy(samp.achSampleName, waveName.c_str(), std::min(waveName.length(), static_cast<size_t>(20)));
    samp.dwStart = sampOffset;
    samp.dwEnd = samp.dwStart + (wav.dataLength / sizeof(uint16_t));
    sampOffset = samp.dwEnd + 46;        // plus the 46 padding samples required by sf2 spec

    // Search through all regions for an associated sampInfo structure with this sample
    rsnd::InstrInfo *instrInfo = nullptr;
    for (size_t j = 0; j < numInstrs; j++) {
      auto instrRegions = bankfile->getInstrRegions(j);

      size_t numRgns = instrRegions.size();
      for (size_t k = 0; k < numRgns; k++) {
        auto instrRegion = instrRegions[k];
        if (instrRegion.instrInfo->waveIdx == i) {
          instrInfo = instrRegion.instrInfo;
          break;
        }
      }
      if (instrInfo != nullptr)
        break;
    }
    //  If we didn't find a rgn association, then idk.
    if (instrInfo == nullptr) {
      std::cout << "Warn: No instrument info for wave index " << i << '\n';
      continue;
    }
    assert (instrInfo != NULL);

    samp.dwStartloop = samp.dwStart + wav.loopStart;
    samp.dwEndloop = samp.dwStart + wav.loopEnd + 1;
    samp.dwSampleRate = wav.sampleRate;
    samp.byOriginalKey = static_cast<uint8_t>(instrInfo->originalKey);
    samp.chCorrection = 0;
    samp.wSampleLink = 0;
    samp.sfSampleType = monoSample; // Do stereo samples exist in RBNKs?

    memcpy(shdrCk->data + (i * sizeof(sfSample)), &samp, sizeof(sfSample));
  }

  //  add terminal sfSample
  memset(shdrCk->data + (numSamps * sizeof(sfSample)), 0, sizeof(sfSample));
  pdtaCk->AddChildChunk(shdrCk);

  this->AddChildChunk(pdtaCk);
}

std::vector<uint8_t> SF2File::SaveToMem() {
  std::vector<uint8_t> buf(GetSize());
  Write(buf.data());
  return buf;
}

bool SF2File::SaveSF2File(const std::filesystem::path &filepath) {
  auto buf = SaveToMem();
  rsnd::writeBinary(filepath, buf.data(), buf.size());
  return true;
}