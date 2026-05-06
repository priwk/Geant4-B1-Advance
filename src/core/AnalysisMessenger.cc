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
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

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

    if (v == "stagec_opticalrve" || v == "opticalrve")
      return RunMode::StageC_OpticalRVE;

    G4Exception("AnalysisMessenger::ParseRunModeOrThrow",
                "BNZS_CFG_001", FatalException,
                ("Unknown run mode: " + raw +
                 ". Supported values: StageA_NeutronPatch, StageB_ReplayAlphaLi, StageC_OpticalStub, StageC_OpticalRVE")
                    .c_str());

    return RunMode::StageB_ReplayAlphaLi;
  }

  bool TryParseRatioFolderName(const std::string &name, G4double &bnWt, G4double &znsWt)
  {
    const std::size_t dashPos = name.find('-');
    if (dashPos == std::string::npos)
      return false;

    try
    {
      bnWt = std::stod(name.substr(0, dashPos));
      znsWt = std::stod(name.substr(dashPos + 1));
    }
    catch (...)
    {
      return false;
    }

    return bnWt > 0.0 && znsWt > 0.0;
  }

  std::vector<std::string> SplitCsvLine(const std::string &line)
  {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
    {
      fields.push_back(field);
    }
    return fields;
  }

  bool ReadFirstStageCSourceMetadata(const std::string &path,
                                     G4double &bnWt,
                                     G4double &znsWt,
                                     std::string &placementFile)
  {
    std::ifstream fin(path.c_str());
    if (!fin)
      return false;

    std::string headerLine;
    if (!std::getline(fin, headerLine))
      return false;

    const auto headers = SplitCsvLine(headerLine);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < headers.size(); ++i)
    {
      index[headers[i]] = i;
    }

    const auto bnIt = index.find("bn_wt");
    const auto znsIt = index.find("zns_wt");
    const auto placementIt = index.find("placement_file");
    if (bnIt == index.end() || znsIt == index.end() || placementIt == index.end())
      return false;

    std::string line;
    while (std::getline(fin, line))
    {
      if (line.empty())
        continue;

      const auto fields = SplitCsvLine(line);
      if (fields.size() <= std::max({bnIt->second, znsIt->second, placementIt->second}))
        continue;

      try
      {
        bnWt = std::stod(fields[bnIt->second]);
        znsWt = std::stod(fields[znsIt->second]);
      }
      catch (...)
      {
        return false;
      }

      placementFile = fields[placementIt->second];
      return bnWt > 0.0 && znsWt > 0.0 && !placementFile.empty();
    }

    return false;
  }

  bool SamePlacementFile(const std::string &lhs, const std::string &rhs)
  {
    if (lhs == rhs)
      return true;

    namespace fs = std::filesystem;
    std::error_code ecL;
    std::error_code ecR;
    const fs::path lhsPath(lhs);
    const fs::path rhsPath(rhs);
    const fs::path lhsCanon = fs::weakly_canonical(lhsPath, ecL);
    const fs::path rhsCanon = fs::weakly_canonical(rhsPath, ecR);
    if (!ecL && !ecR && lhsCanon == rhsCanon)
      return true;

    // Useful when one path is absolute from the VM shared mount and the other
    // is relative from build/. Placement basenames repeat across ratios, so
    // require both ratio folder and filename to match.
    return lhsPath.filename() == rhsPath.filename() &&
           lhsPath.parent_path().filename() == rhsPath.parent_path().filename();
  }

  void RequireMatchingPlacement(const std::string &requested,
                                const std::string &sourcePlacement)
  {
    if (requested.empty() || sourcePlacement.empty())
      return;

    if (SamePlacementFile(requested, sourcePlacement))
      return;

    G4Exception("AnalysisMessenger::RequireMatchingPlacement",
                "BNZS_CFG_007", FatalException,
                ("Stage C placement mismatch. The ZnS source CSV was generated with placement_file="
                 + sourcePlacement + " but requested placementFilePath=" + requested
                 + ". Do not mix placements for StageC_OpticalRVE.")
                    .c_str());
  }
} // namespace

AnalysisMessenger::AnalysisMessenger(AnalysisConfig *config)
    : G4UImessenger(),
      fConfig(config),
      fCfgDir(nullptr),
      fRunModeCmd(nullptr),
      fCaptureCsvCmd(nullptr),
      fCaptureDirCmd(nullptr),
      fOpticalSourceCmd(nullptr),
      fSourceSamplingCmd(nullptr),
      fPlacementFileCmd(nullptr),
      fUseRandomPlacementCmd(nullptr),
      fAllowThicknessEqualCmd(nullptr),
      fWriteStageCPhotonCsvCmd(nullptr),
      fOpticalSamplesPerStepCmd(nullptr),
      fOpticalParamsCmd(nullptr),
      fWeightRatioCmd(nullptr)
{
  fCfgDir = new G4UIdirectory("/cfg/");
  fCfgDir->SetGuidance("Analysis configuration control.");

  fRunModeCmd = new G4UIcmdWithAString("/cfg/setRunMode", this);
  fRunModeCmd->SetGuidance("Set analysis run mode.");
  fRunModeCmd->SetGuidance("Supported: StageA_NeutronPatch | StageB_ReplayAlphaLi | StageC_OpticalStub | StageC_OpticalRVE");
  fRunModeCmd->SetParameterName("runMode", false);
  fRunModeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fCaptureCsvCmd = new G4UIcmdWithAString("/cfg/setCaptureCsv", this);
  fCaptureCsvCmd->SetGuidance("Set explicit capture CSV path for Stage B replay.");
  fCaptureCsvCmd->SetParameterName("captureCsvPath", false);
  fCaptureCsvCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fCaptureDirCmd = new G4UIcmdWithAString("/cfg/setCaptureDir", this);
  fCaptureDirCmd->SetGuidance("Set explicit capture CSV directory for Stage B replay.");
  fCaptureDirCmd->SetParameterName("captureInputDir", false);
  fCaptureDirCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fOpticalSourceCmd = new G4UIcmdWithAString("/cfg/setOpticalSourceCsv", this);
  fOpticalSourceCmd->SetGuidance("Set explicit Stage C ZnS step source CSV path.");
  fOpticalSourceCmd->SetGuidance("If possible, BN/ZnS ratio and placement file are inferred from the first data row.");
  fOpticalSourceCmd->SetParameterName("opticalSourcePath", false);
  fOpticalSourceCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fSourceSamplingCmd = new G4UIcmdWithAString("/cfg/setSourceSampling", this);
  fSourceSamplingCmd->SetGuidance("Set Stage C source sampling: uniformAlongStep or midpoint.");
  fSourceSamplingCmd->SetParameterName("sourceSampling", false);
  fSourceSamplingCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fPlacementFileCmd = new G4UIcmdWithAString("/cfg/setPlacementFile", this);
  fPlacementFileCmd->SetGuidance("Set explicit placement CSV path for geometry construction.");
  fPlacementFileCmd->SetParameterName("placementFilePath", false);
  fPlacementFileCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fUseRandomPlacementCmd = new G4UIcmdWithABool("/cfg/setUseRandomPlacement", this);
  fUseRandomPlacementCmd->SetGuidance("Enable random placement selection from the current BN:ZnS ratio folder.");
  fUseRandomPlacementCmd->SetParameterName("useRandomPlacement", false);
  fUseRandomPlacementCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fAllowThicknessEqualCmd = new G4UIcmdWithABool("/cfg/setAllowThicknessEqualLocalPatch", this);
  fAllowThicknessEqualCmd->SetGuidance("Allow input thickness == local patch thickness in Stage B.");
  fAllowThicknessEqualCmd->SetParameterName("allowEqual", false);
  fAllowThicknessEqualCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fWriteStageCPhotonCsvCmd = new G4UIcmdWithABool("/cfg/setWriteStageCPhotonCsv", this);
  fWriteStageCPhotonCsvCmd->SetGuidance("Enable or disable Stage C per-photon CSV output.");
  fWriteStageCPhotonCsvCmd->SetGuidance("Default is false to avoid very large output files.");
  fWriteStageCPhotonCsvCmd->SetParameterName("writePhotonCsv", false);
  fWriteStageCPhotonCsvCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fOpticalSamplesPerStepCmd = new G4UIcommand("/cfg/setSamplePhotonsPerStep", this);
  fOpticalSamplesPerStepCmd->SetGuidance("Set Stage C sampled optical photons per ZnS step.");
  fOpticalSamplesPerStepCmd->SetGuidance("Each sampled photon weight is n_photon_step / N_sample_photons_per_step.");

  auto *sampleParam = new G4UIparameter("nSamples", 'i', false);
  sampleParam->SetGuidance("Positive integer sample count per source step.");
  fOpticalSamplesPerStepCmd->SetParameter(sampleParam);
  fOpticalSamplesPerStepCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fOpticalParamsCmd = new G4UIcommand("/cfg/setOpticalParams", this);
  fOpticalParamsCmd->SetGuidance("Set Stage C optical material parameters.");
  fOpticalParamsCmd->SetGuidance("Usage: /cfg/setOpticalParams <matrix_n> <matrix_abs_um> <bn_n> <bn_abs_um> <zns_n> <zns_abs_um>");

  auto *matrixNParam = new G4UIparameter("matrixRIndex", 'd', false);
  fOpticalParamsCmd->SetParameter(matrixNParam);
  auto *matrixAbsParam = new G4UIparameter("matrixAbsLengthUm", 'd', false);
  fOpticalParamsCmd->SetParameter(matrixAbsParam);
  auto *bnNParam = new G4UIparameter("bnRIndex", 'd', false);
  fOpticalParamsCmd->SetParameter(bnNParam);
  auto *bnAbsParam = new G4UIparameter("bnAbsLengthUm", 'd', false);
  fOpticalParamsCmd->SetParameter(bnAbsParam);
  auto *znsNParam = new G4UIparameter("znsRIndex", 'd', false);
  fOpticalParamsCmd->SetParameter(znsNParam);
  auto *znsAbsParam = new G4UIparameter("znsAbsLengthUm", 'd', false);
  fOpticalParamsCmd->SetParameter(znsAbsParam);
  fOpticalParamsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

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
  delete fOpticalParamsCmd;
  delete fOpticalSamplesPerStepCmd;
  delete fWriteStageCPhotonCsvCmd;
  delete fAllowThicknessEqualCmd;
  delete fUseRandomPlacementCmd;
  delete fPlacementFileCmd;
  delete fSourceSamplingCmd;
  delete fOpticalSourceCmd;
  delete fCaptureDirCmd;
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

    G4double dirBnWt = 0.0;
    G4double dirZnsWt = 0.0;
    const std::string dirName = std::filesystem::path(fConfig->captureCsvPath).parent_path().filename().string();
    if (TryParseRatioFolderName(dirName, dirBnWt, dirZnsWt))
    {
      fConfig->bnWt = dirBnWt;
      fConfig->znsWt = dirZnsWt;
      G4cout << "[AnalysisMessenger] inferred weight ratio from captureCsvPath: BN:ZnS = "
             << fConfig->bnWt << ":" << fConfig->znsWt
             << G4endl;
    }

    G4cout << "[AnalysisMessenger] captureCsvPath set to "
           << fConfig->captureCsvPath
           << G4endl;
    return;
  }

  if (command == fCaptureDirCmd)
  {
    fConfig->captureInputDir = Trim(newValue);

    G4double dirBnWt = 0.0;
    G4double dirZnsWt = 0.0;
    const std::string dirName = std::filesystem::path(fConfig->captureInputDir).filename().string();
    if (TryParseRatioFolderName(dirName, dirBnWt, dirZnsWt))
    {
      fConfig->bnWt = dirBnWt;
      fConfig->znsWt = dirZnsWt;
      G4cout << "[AnalysisMessenger] inferred weight ratio from captureInputDir: BN:ZnS = "
             << fConfig->bnWt << ":" << fConfig->znsWt
             << G4endl;
    }

    G4cout << "[AnalysisMessenger] captureInputDir set to "
           << fConfig->captureInputDir
           << G4endl;
    return;
  }

  if (command == fOpticalSourceCmd)
  {
    fConfig->opticalSourcePath = Trim(newValue);

    G4double sourceBnWt = 0.0;
    G4double sourceZnsWt = 0.0;
    std::string sourcePlacementFile;
    if (ReadFirstStageCSourceMetadata(fConfig->opticalSourcePath,
                                      sourceBnWt,
                                      sourceZnsWt,
                                      sourcePlacementFile))
    {
      fConfig->bnWt = sourceBnWt;
      fConfig->znsWt = sourceZnsWt;
      RequireMatchingPlacement(fConfig->placementFilePath, sourcePlacementFile);
      fConfig->placementFilePath = sourcePlacementFile;
      fConfig->useRandomPlacement = false;

      G4cout << "[AnalysisMessenger] inferred Stage C geometry from optical source:"
             << " BN:ZnS=" << fConfig->bnWt << ":" << fConfig->znsWt
             << " placement=" << fConfig->placementFilePath
             << G4endl;
    }

    G4cout << "[AnalysisMessenger] opticalSourcePath set to "
           << fConfig->opticalSourcePath
           << G4endl;
    return;
  }

  if (command == fSourceSamplingCmd)
  {
    const std::string lower = ToLowerCopy(Trim(newValue));
    if (lower != "uniformalongstep" && lower != "midpoint")
    {
      G4Exception("AnalysisMessenger::SetNewValue",
                  "BNZS_CFG_005", FatalException,
                  "Stage C sourceSampling must be uniformAlongStep or midpoint.");
      return;
    }

    fConfig->sourceSampling = (lower == "midpoint") ? "midpoint" : "uniformAlongStep";
    G4cout << "[AnalysisMessenger] sourceSampling set to "
           << fConfig->sourceSampling
           << G4endl;
    return;
  }

  if (command == fPlacementFileCmd)
  {
    const std::string requestedPlacement = Trim(newValue);
    if (!fConfig->opticalSourcePath.empty())
    {
      G4double sourceBnWt = 0.0;
      G4double sourceZnsWt = 0.0;
      std::string sourcePlacementFile;
      if (ReadFirstStageCSourceMetadata(fConfig->opticalSourcePath,
                                        sourceBnWt,
                                        sourceZnsWt,
                                        sourcePlacementFile))
      {
        RequireMatchingPlacement(requestedPlacement, sourcePlacementFile);
      }
    }

    fConfig->placementFilePath = requestedPlacement;
    fConfig->useRandomPlacement = false;

    G4cout << "[AnalysisMessenger] placementFilePath set to "
           << fConfig->placementFilePath
           << " ; useRandomPlacement=false"
           << G4endl;
    return;
  }

  if (command == fUseRandomPlacementCmd)
  {
    fConfig->useRandomPlacement =
        fUseRandomPlacementCmd->GetNewBoolValue(newValue);

    G4cout << "[AnalysisMessenger] useRandomPlacement set to "
           << (fConfig->useRandomPlacement ? "true" : "false")
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
  if (command == fWriteStageCPhotonCsvCmd)
  {
    fConfig->writeStageCPhotonCsv =
        fWriteStageCPhotonCsvCmd->GetNewBoolValue(newValue);

    G4cout << "[AnalysisMessenger] writeStageCPhotonCsv set to "
           << (fConfig->writeStageCPhotonCsv ? "true" : "false")
           << G4endl;
    return;
  }
  if (command == fOpticalSamplesPerStepCmd)
  {
    std::istringstream iss(Trim(newValue));
    G4int samples = 0;

    if (!(iss >> samples) || samples <= 0)
    {
      G4Exception("AnalysisMessenger::SetNewValue",
                  "BNZS_CFG_006", FatalException,
                  "Sample photons per step must be a positive integer.");
      return;
    }

    fConfig->opticalSamplesPerStep = samples;
    G4cout << "[AnalysisMessenger] opticalSamplesPerStep set to "
           << fConfig->opticalSamplesPerStep
           << G4endl;
    return;
  }
  if (command == fOpticalParamsCmd)
  {
    std::istringstream iss(Trim(newValue));
    G4double matrixN = 0.0;
    G4double matrixAbsUm = 0.0;
    G4double bnN = 0.0;
    G4double bnAbsUm = 0.0;
    G4double znsN = 0.0;
    G4double znsAbsUm = 0.0;

    if (!(iss >> matrixN >> matrixAbsUm >> bnN >> bnAbsUm >> znsN >> znsAbsUm) ||
        matrixN <= 0.0 || matrixAbsUm <= 0.0 ||
        bnN <= 0.0 || bnAbsUm <= 0.0 ||
        znsN <= 0.0 || znsAbsUm <= 0.0)
    {
      G4Exception("AnalysisMessenger::SetNewValue",
                  "BNZS_CFG_008", FatalException,
                  "Optical params must be six positive numbers: matrix_n matrix_abs_um bn_n bn_abs_um zns_n zns_abs_um.");
      return;
    }

    fConfig->opticalMatrixRIndex = matrixN;
    fConfig->opticalMatrixAbsLengthUm = matrixAbsUm;
    fConfig->opticalBnRIndex = bnN;
    fConfig->opticalBnAbsLengthUm = bnAbsUm;
    fConfig->opticalZnsRIndex = znsN;
    fConfig->opticalZnsAbsLengthUm = znsAbsUm;
    fConfig->opticalParamsProvided = true;

    G4cout << "[AnalysisMessenger] optical params set:"
           << " matrix n/abs_um=" << matrixN << "/" << matrixAbsUm
           << " BN n/abs_um=" << bnN << "/" << bnAbsUm
           << " ZnS n/abs_um=" << znsN << "/" << znsAbsUm
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
