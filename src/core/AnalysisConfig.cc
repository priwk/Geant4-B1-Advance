#include "AnalysisConfig.hh"

#include <cstdlib>
#include <string>
#include <algorithm>

namespace
{
  std::string ToLowerCopy(std::string s)
  {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return s;
  }

  bool IsTruthyEnv(const char *value)
  {
    if (value == nullptr)
      return false;

    const std::string v = ToLowerCopy(value);
    return v == "1" || v == "true" || v == "yes" || v == "on";
  }
}

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
  const char *runModeEnv = std::getenv("BNZS_RUN_MODE");
  if (runModeEnv != nullptr)
  {
    const std::string mode = ToLowerCopy(runModeEnv);
    if (mode == "stagea" || mode == "a" || mode == "stagea_neutronpatch")
    {
      runMode = RunMode::StageA_NeutronPatch;
    }
    else if (mode == "stageb" || mode == "b" || mode == "stageb_replayalphali")
    {
      runMode = RunMode::StageB_ReplayAlphaLi;
    }
    else if (mode == "stagec" || mode == "c" || mode == "stagec_opticalstub")
    {
      runMode = RunMode::StageC_OpticalStub;
    }
  }

  const char *placementEnv = std::getenv("BNZS_PLACEMENT_FILE");
  if (placementEnv != nullptr && std::string(placementEnv).size() > 0)
  {
    placementFilePath = placementEnv;
    useRandomPlacement = false;
  }

  const char *randomPlacementEnv = std::getenv("BNZS_USE_RANDOM_PLACEMENT");
  if (randomPlacementEnv != nullptr)
  {
    useRandomPlacement = IsTruthyEnv(randomPlacementEnv);
  }

  const char *captureCsvEnv = std::getenv("BNZS_INPUT_CSV");
  if (captureCsvEnv != nullptr && std::string(captureCsvEnv).size() > 0)
  {
    captureCsvPath = captureCsvEnv;
  }
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
