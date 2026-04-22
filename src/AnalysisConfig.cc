#include "AnalysisConfig.hh"

AnalysisConfig::AnalysisConfig()
    : runMode(RunMode::StageB_ReplayAlphaLi),
      patchXY_um(50.0),
      microThickness_um(30.0),
      bnWt(1.0),
      znsWt(2.0),
      useRandomPlacement(true),
      placementFilePath(""),
      captureCsvPath(""),
      opticalSourcePath(""),
      allowThicknessEqualLocalPatch(true)
{
}

AnalysisConfig::~AnalysisConfig() = default;

const char *AnalysisConfig::RunModeName(RunMode mode)
{
  switch (mode)
  {
  case RunMode::StageA_NeutronPatch:
    return "StageA_NeutronPatch";
  case RunMode::StageB_ReplayAlphaLi:
    return "StageB_ReplayAlphaLi";
  case RunMode::StageC_OpticalStub:
    return "StageC_OpticalStub";
  default:
    return "UnknownRunMode";
  }
}