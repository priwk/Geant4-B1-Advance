#include "StageDOpticalRunAction.hh"

#include "AnalysisConfig.hh"
#include "DetectorConstruction.hh"

#include "G4Exception.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace
{
  std::string WeightPartToTagString(G4double value)
  {
    const G4double rounded = std::round(value);
    std::ostringstream oss;
    if (std::abs(value - rounded) < 1.0e-9)
      oss << static_cast<long long>(rounded);
    else
      oss << value;
    return oss.str();
  }

  std::string CsvQuote(const std::string &value)
  {
    if (value.find_first_of(",\"") == std::string::npos)
      return value;
    std::string out = "\"";
    for (char ch : value)
    {
      if (ch == '"')
        out += "\"\"";
      else
        out += ch;
    }
    out += "\"";
    return out;
  }
}

StageDOpticalRunAction::StageDOpticalRunAction(AnalysisConfig *config)
    : G4UserRunAction(),
      fConfig(config),
      fEventsCsv(),
      fEventsCsvPath(""),
      fSummaryCsvPath(""),
      fOutputDir(""),
      fRatioTag(""),
      fPlacementFile(""),
      fPlacementStem(""),
      fEvents()
{
}

StageDOpticalRunAction::~StageDOpticalRunAction()
{
  if (fEventsCsv.is_open())
    fEventsCsv.close();
}

std::string StageDOpticalRunAction::MakeRatioTag() const
{
  if (fConfig == nullptr)
    return "unknown";
  return WeightPartToTagString(fConfig->bnWt) + "-" +
         WeightPartToTagString(fConfig->znsWt);
}

std::string StageDOpticalRunAction::MakePlacementStem() const
{
  if (fPlacementFile.empty())
    return "unknown_placement";
  return std::filesystem::path(fPlacementFile).stem().string();
}

std::string StageDOpticalRunAction::ResolveOutputDirectory() const
{
  if (fConfig != nullptr && !fConfig->stageD_output_dir.empty())
    return fConfig->stageD_output_dir;

  return (AnalysisConfig::ProjectRootPath() /
          "Output" /
          "stageD_optical_homogenization" /
          fRatioTag /
          fPlacementStem)
      .string();
}

void StageDOpticalRunAction::WriteEventHeader()
{
  fEventsCsv
      << "photonID,"
      << "ratio,"
      << "placement_file,"
      << "source_mode,"
      << "boundary_mode,"
      << "reentry_mode,"
      << "matrix_reentry_mode,"
      << "wavelength_nm,"
      << "source_phase,"
      << "source_x_um,"
      << "source_y_um,"
      << "source_z_um,"
      << "final_status,"
      << "absorbed,"
      << "total_path_length_um,"
      << "num_steps,"
      << "num_real_scatter,"
      << "num_material_boundary,"
      << "num_reentry,"
      << "num_reentry_BN,"
      << "num_reentry_ZnS,"
      << "num_reentry_matrix,"
      << "sum_cos_theta,"
      << "mean_cos_theta_for_this_photon,"
      << "weight"
      << "\n";
}

void StageDOpticalRunAction::OpenOutputs()
{
  std::error_code ec;
  std::filesystem::create_directories(fOutputDir, ec);
  if (ec)
  {
    G4Exception("StageDOpticalRunAction::OpenOutputs",
                "BNZS_D_RUN_001", FatalException,
                ("Failed to create Stage D output directory: " + fOutputDir).c_str());
    return;
  }

  fEventsCsvPath = (std::filesystem::path(fOutputDir) /
                    "optical_homogenization_events.csv")
                       .string();
  fSummaryCsvPath = (std::filesystem::path(fOutputDir) /
                     "optical_homogenization_summary.csv")
                        .string();

  fEventsCsv.open(fEventsCsvPath.c_str(), std::ios::out);
  if (!fEventsCsv)
  {
    G4Exception("StageDOpticalRunAction::OpenOutputs",
                "BNZS_D_RUN_002", FatalException,
                ("Failed to open Stage D events CSV: " + fEventsCsvPath).c_str());
    return;
  }

  WriteEventHeader();
}

void StageDOpticalRunAction::BeginOfRunAction(const G4Run *run)
{
  (void)run;

  fEvents.clear();
  if (fEventsCsv.is_open())
    fEventsCsv.close();

  const auto *detector = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  fRatioTag = MakeRatioTag();
  fPlacementFile = detector ? detector->GetLoadedPlacementFileForRecord() : "unknown";
  fPlacementStem = MakePlacementStem();
  fOutputDir = ResolveOutputDirectory();

  OpenOutputs();
}

void StageDOpticalRunAction::RecordPhotonEvent(const StageDPhotonEventRecord &event)
{
  fEvents.push_back(event);

  if (!fEventsCsv.is_open())
    return;

  fEventsCsv
      << event.photonID << ","
      << CsvQuote(event.ratio) << ","
      << CsvQuote(event.placement_file) << ","
      << CsvQuote(event.source_mode) << ","
      << CsvQuote(event.boundary_mode) << ","
      << CsvQuote(event.reentry_mode) << ","
      << CsvQuote(event.matrix_reentry_mode) << ","
      << event.wavelength_nm << ","
      << CsvQuote(event.source_phase) << ","
      << event.source_x_um << ","
      << event.source_y_um << ","
      << event.source_z_um << ","
      << CsvQuote(event.final_status) << ","
      << (event.absorbed ? 1 : 0) << ","
      << event.total_path_length_um << ","
      << event.num_steps << ","
      << event.num_real_scatter << ","
      << event.num_material_boundary << ","
      << event.num_reentry << ","
      << event.num_reentry_BN << ","
      << event.num_reentry_ZnS << ","
      << event.num_reentry_matrix << ","
      << event.sum_cos_theta << ","
      << event.mean_cos_theta_for_this_photon << ","
      << event.weight
      << "\n";
}

void StageDOpticalRunAction::WriteSummaryFile() const
{
  std::ofstream fout(fSummaryCsvPath.c_str(), std::ios::out);
  if (!fout)
  {
    G4Exception("StageDOpticalRunAction::WriteSummaryFile",
                "BNZS_D_RUN_003", FatalException,
                ("Failed to open Stage D summary CSV: " + fSummaryCsvPath).c_str());
    return;
  }

  G4long nPhotons = static_cast<G4long>(fEvents.size());
  G4long nAbsorbed = 0;
  G4long nLost = 0;
  G4double totalPathLengthUm = 0.0;
  G4long totalRealScatter = 0;
  G4long totalReentry = 0;
  G4long totalReentryBN = 0;
  G4long totalReentryZnS = 0;
  G4long totalReentryMatrix = 0;
  G4double sumCosThetaAllScatter = 0.0;

  for (const auto &event : fEvents)
  {
    if (event.absorbed)
      ++nAbsorbed;
    if (event.final_status == "lost" || event.final_status == "reentry_failed")
      ++nLost;

    totalPathLengthUm += event.total_path_length_um;
    totalRealScatter += event.num_real_scatter;
    totalReentry += event.num_reentry;
    totalReentryBN += event.num_reentry_BN;
    totalReentryZnS += event.num_reentry_ZnS;
    totalReentryMatrix += event.num_reentry_matrix;
    sumCosThetaAllScatter += event.sum_cos_theta;
  }

  const G4double nPhotonsD = (nPhotons > 0) ? static_cast<G4double>(nPhotons) : 1.0;
  const G4double muA = (totalPathLengthUm > 0.0)
                           ? static_cast<G4double>(nAbsorbed) / totalPathLengthUm
                           : 0.0;
  const G4double muS = (totalPathLengthUm > 0.0)
                           ? static_cast<G4double>(totalRealScatter) / totalPathLengthUm
                           : 0.0;
  const G4double gRaw = (totalRealScatter > 0)
                            ? sumCosThetaAllScatter / static_cast<G4double>(totalRealScatter)
                            : 0.0;
  const G4double muSPrime = muS * (1.0 - gRaw);

  fout
      << "ratio,"
      << "placement_file,"
      << "source_mode,"
      << "boundary_mode,"
      << "reentry_mode,"
      << "matrix_reentry_mode,"
      << "wavelength_nm,"
      << "n_photons,"
      << "n_absorbed,"
      << "absorbed_fraction,"
      << "n_lost,"
      << "lost_fraction,"
      << "total_path_length_um,"
      << "mean_path_length_um,"
      << "total_real_scatter,"
      << "mean_num_real_scatter,"
      << "total_reentry,"
      << "mean_num_reentry,"
      << "total_reentry_BN,"
      << "total_reentry_ZnS,"
      << "total_reentry_matrix,"
      << "mu_a_raw_per_um,"
      << "mu_s_raw_per_um,"
      << "g_raw,"
      << "mu_s_prime_raw_per_um,"
      << "theta_threshold_deg,"
      << "max_reentry,"
      << "max_steps,"
      << "max_path_length_um"
      << "\n";

  fout
      << CsvQuote(fRatioTag) << ","
      << CsvQuote(fPlacementFile) << ","
      << CsvQuote(fConfig ? fConfig->stageD_source_mode : "") << ","
      << CsvQuote(fConfig ? fConfig->stageD_boundary_mode : "") << ","
      << CsvQuote(fConfig ? fConfig->stageD_reentry_mode : "") << ","
      << CsvQuote(fConfig ? fConfig->stageD_matrix_reentry_mode : "") << ","
      << (fConfig ? fConfig->stageD_wavelength_nm : 0.0) << ","
      << nPhotons << ","
      << nAbsorbed << ","
      << static_cast<G4double>(nAbsorbed) / nPhotonsD << ","
      << nLost << ","
      << static_cast<G4double>(nLost) / nPhotonsD << ","
      << totalPathLengthUm << ","
      << totalPathLengthUm / nPhotonsD << ","
      << totalRealScatter << ","
      << static_cast<G4double>(totalRealScatter) / nPhotonsD << ","
      << totalReentry << ","
      << static_cast<G4double>(totalReentry) / nPhotonsD << ","
      << totalReentryBN << ","
      << totalReentryZnS << ","
      << totalReentryMatrix << ","
      << muA << ","
      << muS << ","
      << gRaw << ","
      << muSPrime << ","
      << (fConfig ? fConfig->stageD_theta_threshold_deg : 0.0) << ","
      << (fConfig ? fConfig->stageD_max_reentry : 0) << ","
      << (fConfig ? fConfig->stageD_max_steps : 0) << ","
      << (fConfig ? fConfig->stageD_max_path_length_um : 0.0)
      << "\n";
}

void StageDOpticalRunAction::EndOfRunAction(const G4Run *run)
{
  (void)run;

  if (fEventsCsv.is_open())
    fEventsCsv.close();

  WriteSummaryFile();

  G4cout << "[StageDOpticalRunAction] End run"
         << "\n  events csv  = " << fEventsCsvPath
         << "\n  summary csv = " << fSummaryCsvPath
         << "\n  n photons   = " << fEvents.size()
         << G4endl;
}
