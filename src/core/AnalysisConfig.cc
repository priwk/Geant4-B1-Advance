#include "AnalysisConfig.hh"

#include <cstdlib>
#include <cctype>
#include <string>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace
{
  std::filesystem::path DetectProjectRoot()
  {
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (ec)
      return fs::path(".");

    std::vector<fs::path> candidates;
    candidates.push_back(cwd);
    if (cwd.filename() == "build" && cwd.has_parent_path())
      candidates.push_back(cwd.parent_path());
    if (cwd.has_parent_path())
      candidates.push_back(cwd.parent_path());

    for (const auto &candidate : candidates)
    {
      if (candidate.empty())
        continue;
      if (fs::exists(candidate / "CMakeLists.txt", ec) && !ec)
        return candidate;
      ec.clear();
      if (fs::exists(candidate / "README.md", ec) && !ec)
        return candidate;
      ec.clear();
    }

    return (cwd.filename() == "build" && cwd.has_parent_path()) ? cwd.parent_path() : cwd;
  }

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

  bool TryParseRatioFolderName(const std::string &name, double &bnWt, double &znsWt)
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
                                     double &bnWt,
                                     double &znsWt,
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
}

std::filesystem::path AnalysisConfig::ProjectRootPath()
{
  return DetectProjectRoot();
}

std::string AnalysisConfig::PathForRecord(const std::filesystem::path &path)
{
  namespace fs = std::filesystem;

  if (path.empty())
    return "";

  const fs::path projectRoot = ProjectRootPath();
  std::error_code ec;

  fs::path normalized = path;
  if (path.is_absolute())
    normalized = fs::weakly_canonical(path, ec);
  else
    normalized = (projectRoot / path).lexically_normal();

  if (ec)
  {
    ec.clear();
    normalized = path.lexically_normal();
  }

  const fs::path normalizedRoot = fs::weakly_canonical(projectRoot, ec);
  if (ec)
  {
    ec.clear();
    return normalized.generic_string();
  }

  const fs::path relative = normalized.lexically_relative(normalizedRoot);
  if (!relative.empty() && relative.native().find("..") != 0)
    return relative.generic_string();

  return normalized.generic_string();
}

std::string AnalysisConfig::PathForRecord(const std::string &path)
{
  if (path.empty())
    return "";
  return PathForRecord(std::filesystem::path(path));
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
      captureInputDir(""),
      opticalSourcePath(""),
      sourceSampling("uniformAlongStep"),
      opticalSamplesPerStep(1),
      opticalParamsProvided(false),
      opticalMatrixRIndex(1.5),
      opticalMatrixAbsLengthUm(1.0e6),
      opticalBnRIndex(2.1),
      opticalBnAbsLengthUm(10.0),
      opticalZnsRIndex(2.36),
      opticalZnsAbsLengthUm(50.0),
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
    else if (mode == "stagec_opticalrve" || mode == "opticalrve")
    {
      runMode = RunMode::StageC_OpticalRVE;
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

    double dirBnWt = 0.0;
    double dirZnsWt = 0.0;
    const std::string dirName = std::filesystem::path(captureCsvPath).parent_path().filename().string();
    if (TryParseRatioFolderName(dirName, dirBnWt, dirZnsWt))
    {
      bnWt = dirBnWt;
      znsWt = dirZnsWt;
    }
  }

  const char *captureDirEnv = std::getenv("BNZS_INPUT_DIR");
  if (captureDirEnv != nullptr && std::string(captureDirEnv).size() > 0)
  {
    captureInputDir = captureDirEnv;

    double dirBnWt = 0.0;
    double dirZnsWt = 0.0;
    const std::string dirName = std::filesystem::path(captureInputDir).filename().string();
    if (TryParseRatioFolderName(dirName, dirBnWt, dirZnsWt))
    {
      bnWt = dirBnWt;
      znsWt = dirZnsWt;
    }
  }

  const char *opticalSourceEnv = std::getenv("BNZS_OPTICAL_SOURCE_CSV");
  if (opticalSourceEnv == nullptr || std::string(opticalSourceEnv).empty())
  {
    opticalSourceEnv = std::getenv("BNZS_STAGEC_SOURCE_CSV");
  }
  if (opticalSourceEnv != nullptr && std::string(opticalSourceEnv).size() > 0)
  {
    opticalSourcePath = opticalSourceEnv;

    double sourceBnWt = 0.0;
    double sourceZnsWt = 0.0;
    std::string sourcePlacementFile;
    if (ReadFirstStageCSourceMetadata(opticalSourcePath,
                                      sourceBnWt,
                                      sourceZnsWt,
                                      sourcePlacementFile))
    {
      bnWt = sourceBnWt;
      znsWt = sourceZnsWt;
      placementFilePath = sourcePlacementFile;
      useRandomPlacement = false;
    }
  }

  const char *sourceSamplingEnv = std::getenv("BNZS_SOURCE_SAMPLING");
  if (sourceSamplingEnv != nullptr && std::string(sourceSamplingEnv).size() > 0)
  {
    sourceSampling = sourceSamplingEnv;
  }

  const char *samplesEnv = std::getenv("BNZS_SAMPLE_PHOTONS_PER_STEP");
  if (samplesEnv != nullptr && std::string(samplesEnv).size() > 0)
  {
    try
    {
      const int samples = std::stoi(samplesEnv);
      if (samples > 0)
      {
        opticalSamplesPerStep = samples;
      }
    }
    catch (...)
    {
    }
  }

  auto readOpticalDoubleEnv = [&](const char *name, double &target)
  {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string(value).empty())
      return;
    try
    {
      const double parsed = std::stod(value);
      if (parsed > 0.0)
      {
        target = parsed;
        opticalParamsProvided = true;
      }
    }
    catch (...)
    {
    }
  };

  readOpticalDoubleEnv("BNZS_OPTICAL_MATRIX_RINDEX", opticalMatrixRIndex);
  readOpticalDoubleEnv("BNZS_OPTICAL_MATRIX_ABSLENGTH_UM", opticalMatrixAbsLengthUm);
  readOpticalDoubleEnv("BNZS_OPTICAL_BN_RINDEX", opticalBnRIndex);
  readOpticalDoubleEnv("BNZS_OPTICAL_BN_ABSLENGTH_UM", opticalBnAbsLengthUm);
  readOpticalDoubleEnv("BNZS_OPTICAL_ZNS_RINDEX", opticalZnsRIndex);
  readOpticalDoubleEnv("BNZS_OPTICAL_ZNS_ABSLENGTH_UM", opticalZnsAbsLengthUm);

  const char *bnWtEnv = std::getenv("BNZS_BN_WT");
  const char *znsWtEnv = std::getenv("BNZS_ZNS_WT");
  if (bnWtEnv != nullptr && znsWtEnv != nullptr)
  {
    try
    {
      const double envBnWt = std::stod(bnWtEnv);
      const double envZnsWt = std::stod(znsWtEnv);
      if (envBnWt > 0.0 && envZnsWt > 0.0)
      {
        bnWt = envBnWt;
        znsWt = envZnsWt;
      }
    }
    catch (...)
    {
    }
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
  case RunMode::StageC_OpticalRVE:
    return "StageC_OpticalRVE";
  default:
    return "UnknownRunMode";
  }
}
