#include "AnalysisMessenger.hh"

#include "AnalysisConfig.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcommand.hh"
#include "G4UIparameter.hh"
#include "G4ios.hh"
#include "G4Exception.hh"

#include <algorithm>
#include <cctype>
#include <string>
#include <sstream>

namespace
{
  std::string Trim(const std::string &s)
  {
    const auto first = std::find_if_not(s.begin(), s.end(),
                                        [](unsigned char ch)
                                        { return std::isspace(ch); });
    const auto last = std::find_if_not(s.rbegin(), s.rend(),
                                       [](unsigned char ch)
                                       { return std::isspace(ch); })
                          .base();
    if (first >= last)
      return "";
    return std::string(first, last);
  }

  std::string ToLowerCopy(std::string s)
  {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return s;
  }

  RunMode ParseRunModeOrThrow(const std::string &raw)
  {
    const std::string v = ToLowerCopy(Trim(raw));

    if (v == "stagea_neutronpatch" || v == "stagea" || v == "a")
      return RunMode::StageA_NeutronPatch;

    if (v == "stageb_replayalphali" || v == "stageb" || v == "b")
      return RunMode::StageB_ReplayAlphaLi;

    if (v == "stagec_opticalstub" || v == "stagec" || v == "c")
      return RunMode::StageC_OpticalStub;

    G4Exception("AnalysisMessenger::ParseRunModeOrThrow",
                "BNZS_CFG_001", FatalException,
                ("Unknown run mode: " + raw +
                 ". Supported values: StageA_NeutronPatch, StageB_ReplayAlphaLi, StageC_OpticalStub")
                    .c_str());

    return RunMode::StageB_ReplayAlphaLi;
  }
} // namespace

AnalysisMessenger::AnalysisMessenger(AnalysisConfig *config)
    : G4UImessenger(),
      fConfig(config),
      fCfgDir(nullptr),
      fRunModeCmd(nullptr),
      fCaptureCsvCmd(nullptr),
      fAllowThicknessEqualCmd(nullptr),
      fWeightRatioCmd(nullptr)
{
  fCfgDir = new G4UIdirectory("/cfg/");
  fCfgDir->SetGuidance("Analysis configuration control.");

  fRunModeCmd = new G4UIcmdWithAString("/cfg/setRunMode", this);
  fRunModeCmd->SetGuidance("Set analysis run mode.");
  fRunModeCmd->SetGuidance("Supported: StageA_NeutronPatch | StageB_ReplayAlphaLi | StageC_OpticalStub");
  fRunModeCmd->SetParameterName("runMode", false);
  fRunModeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fCaptureCsvCmd = new G4UIcmdWithAString("/cfg/setCaptureCsv", this);
  fCaptureCsvCmd->SetGuidance("Set explicit capture CSV path for Stage B replay.");
  fCaptureCsvCmd->SetParameterName("captureCsvPath", false);
  fCaptureCsvCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fAllowThicknessEqualCmd = new G4UIcmdWithABool("/cfg/setAllowThicknessEqualLocalPatch", this);
  fAllowThicknessEqualCmd->SetGuidance("Allow input thickness == local patch thickness in Stage B.");
  fAllowThicknessEqualCmd->SetParameterName("allowEqual", false);
  fAllowThicknessEqualCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fWeightRatioCmd = new G4UIcommand("/cfg/setWeightRatio", this);
  fWeightRatioCmd->SetGuidance("Set BN:ZnS weight ratio used for geometry construction.");
  fWeightRatioCmd->SetGuidance("Usage: /cfg/setWeightRatio <bnWt> <znsWt>");

  auto *bnParam = new G4UIparameter("bnWt", 'd', false);
  bnParam->SetGuidance("BN weight part, e.g. 1");
  fWeightRatioCmd->SetParameter(bnParam);

  auto *znsParam = new G4UIparameter("znsWt", 'd', false);
  znsParam->SetGuidance("ZnS weight part, e.g. 2");
  fWeightRatioCmd->SetParameter(znsParam);

  fWeightRatioCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

AnalysisMessenger::~AnalysisMessenger()
{
  delete fWeightRatioCmd;
  delete fAllowThicknessEqualCmd;
  delete fCaptureCsvCmd;
  delete fRunModeCmd;
  delete fCfgDir;
}

void AnalysisMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
  if (fConfig == nullptr)
  {
    G4Exception("AnalysisMessenger::SetNewValue",
                "BNZS_CFG_002", FatalException,
                "AnalysisConfig pointer is null.");
    return;
  }

  if (command == fRunModeCmd)
  {
    fConfig->runMode = ParseRunModeOrThrow(newValue);

    G4cout << "[AnalysisMessenger] runMode set to "
           << AnalysisConfig::RunModeName(fConfig->runMode)
           << G4endl;
    return;
  }

  if (command == fCaptureCsvCmd)
  {
    fConfig->captureCsvPath = Trim(newValue);

    G4cout << "[AnalysisMessenger] captureCsvPath set to "
           << fConfig->captureCsvPath
           << G4endl;
    return;
  }

  if (command == fAllowThicknessEqualCmd)
  {
    fConfig->allowThicknessEqualLocalPatch =
        fAllowThicknessEqualCmd->GetNewBoolValue(newValue);

    G4cout << "[AnalysisMessenger] allowThicknessEqualLocalPatch set to "
           << (fConfig->allowThicknessEqualLocalPatch ? "true" : "false")
           << G4endl;
    return;
  }
  if (command == fWeightRatioCmd)
  {
    std::istringstream iss(Trim(newValue));
    G4double bnWt = 0.0;
    G4double znsWt = 0.0;

    if (!(iss >> bnWt >> znsWt))
    {
      G4Exception("AnalysisMessenger::SetNewValue",
                  "BNZS_CFG_003", FatalException,
                  ("Failed to parse /cfg/setWeightRatio arguments: " + std::string(newValue)).c_str());
      return;
    }

    if (bnWt <= 0.0 || znsWt <= 0.0)
    {
      G4Exception("AnalysisMessenger::SetNewValue",
                  "BNZS_CFG_004", FatalException,
                  "BN and ZnS weight parts must both be > 0.");
      return;
    }

    fConfig->bnWt = bnWt;
    fConfig->znsWt = znsWt;

    G4cout << "[AnalysisMessenger] weight ratio set to BN:ZnS = "
           << fConfig->bnWt << ":" << fConfig->znsWt
           << G4endl;
    return;
  }
}