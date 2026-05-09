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
      << "num_particle_scatter,"
      << "num_particle_scatter_BN,"
      << "num_particle_scatter_ZnS,"
      << "num_real_scatter,"
      << "num_bulk_scatter,"
      << "num_boundary_scatter,"
      << "num_boundary_scatter_BN,"
      << "num_boundary_scatter_ZnS,"
      << "num_material_boundary,"
      << "num_reentry,"
      << "num_reentry_BN,"
      << "num_reentry_ZnS,"
      << "num_reentry_matrix,"
      << "sum_cos_theta_particle,"
      << "sum_cos_theta_particle_BN,"
      << "sum_cos_theta_particle_ZnS,"
      << "sum_cos_theta,"
      << "sum_cos_theta_bulk,"
      << "sum_cos_theta_boundary,"
      << "sum_cos_theta_boundary_BN,"
      << "sum_cos_theta_boundary_ZnS,"
      << "mean_cos_theta_particle_for_this_photon,"
      << "mean_cos_theta_for_this_photon,"
      << "mean_cos_theta_bulk_for_this_photon,"
      << "mean_cos_theta_boundary_for_this_photon,"
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
      << event.num_particle_scatter << ","
      << event.num_particle_scatter_BN << ","
      << event.num_particle_scatter_ZnS << ","
      << event.num_real_scatter << ","
      << event.num_bulk_scatter << ","
      << event.num_boundary_scatter << ","
      << event.num_boundary_scatter_BN << ","
      << event.num_boundary_scatter_ZnS << ","
      << event.num_material_boundary << ","
      << event.num_reentry << ","
      << event.num_reentry_BN << ","
      << event.num_reentry_ZnS << ","
      << event.num_reentry_matrix << ","
      << event.sum_cos_theta_particle << ","
      << event.sum_cos_theta_particle_BN << ","
      << event.sum_cos_theta_particle_ZnS << ","
      << event.sum_cos_theta << ","
      << event.sum_cos_theta_bulk << ","
      << event.sum_cos_theta_boundary << ","
      << event.sum_cos_theta_boundary_BN << ","
      << event.sum_cos_theta_boundary_ZnS << ","
      << event.mean_cos_theta_particle_for_this_photon << ","
      << event.mean_cos_theta_for_this_photon << ","
      << event.mean_cos_theta_bulk_for_this_photon << ","
      << event.mean_cos_theta_boundary_for_this_photon << ","
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
  G4long totalParticleScatter = 0;
  G4long totalParticleScatterBN = 0;
  G4long totalParticleScatterZnS = 0;
  G4long totalRealScatter = 0;
  G4long totalBulkScatter = 0;
  G4long totalBoundaryScatter = 0;
  G4long totalBoundaryScatterBN = 0;
  G4long totalBoundaryScatterZnS = 0;
  G4long totalReentry = 0;
  G4long totalReentryBN = 0;
  G4long totalReentryZnS = 0;
  G4long totalReentryMatrix = 0;
  G4double sumCosThetaParticleScatter = 0.0;
  G4double sumCosThetaParticleScatterBN = 0.0;
  G4double sumCosThetaParticleScatterZnS = 0.0;
  G4double sumCosThetaAllScatter = 0.0;
  G4double sumCosThetaBulkScatter = 0.0;
  G4double sumCosThetaBoundaryScatter = 0.0;
  G4double sumCosThetaBoundaryScatterBN = 0.0;
  G4double sumCosThetaBoundaryScatterZnS = 0.0;

  for (const auto &event : fEvents)
  {
    if (event.absorbed)
      ++nAbsorbed;
    if (event.final_status == "lost" || event.final_status == "reentry_failed")
      ++nLost;

    totalPathLengthUm += event.total_path_length_um;
    totalParticleScatter += event.num_particle_scatter;
    totalParticleScatterBN += event.num_particle_scatter_BN;
    totalParticleScatterZnS += event.num_particle_scatter_ZnS;
    totalRealScatter += event.num_real_scatter;
    totalBulkScatter += event.num_bulk_scatter;
    totalBoundaryScatter += event.num_boundary_scatter;
    totalBoundaryScatterBN += event.num_boundary_scatter_BN;
    totalBoundaryScatterZnS += event.num_boundary_scatter_ZnS;
    totalReentry += event.num_reentry;
    totalReentryBN += event.num_reentry_BN;
    totalReentryZnS += event.num_reentry_ZnS;
    totalReentryMatrix += event.num_reentry_matrix;
    sumCosThetaParticleScatter += event.sum_cos_theta_particle;
    sumCosThetaParticleScatterBN += event.sum_cos_theta_particle_BN;
    sumCosThetaParticleScatterZnS += event.sum_cos_theta_particle_ZnS;
    sumCosThetaAllScatter += event.sum_cos_theta;
    sumCosThetaBulkScatter += event.sum_cos_theta_bulk;
    sumCosThetaBoundaryScatter += event.sum_cos_theta_boundary;
    sumCosThetaBoundaryScatterBN += event.sum_cos_theta_boundary_BN;
    sumCosThetaBoundaryScatterZnS += event.sum_cos_theta_boundary_ZnS;
  }

  const G4double nPhotonsD = (nPhotons > 0) ? static_cast<G4double>(nPhotons) : 1.0;
  const std::string scatterMetric = fConfig ? fConfig->stageD_scatter_metric : "";
  const G4double muA = (totalPathLengthUm > 0.0)
                           ? static_cast<G4double>(nAbsorbed) / totalPathLengthUm
                           : 0.0;
  const G4double muSParticle = (totalPathLengthUm > 0.0)
                                   ? static_cast<G4double>(totalParticleScatter) / totalPathLengthUm
                                   : 0.0;
  const G4double muSBN = (totalPathLengthUm > 0.0)
                             ? static_cast<G4double>(totalParticleScatterBN) / totalPathLengthUm
                             : 0.0;
  const G4double muSZnS = (totalPathLengthUm > 0.0)
                              ? static_cast<G4double>(totalParticleScatterZnS) / totalPathLengthUm
                              : 0.0;
  const G4double muSBoundaryPrimary = (totalPathLengthUm > 0.0)
                                          ? static_cast<G4double>(totalBoundaryScatter) / totalPathLengthUm
                                          : 0.0;
  const G4double muSBoundaryBN = (totalPathLengthUm > 0.0)
                                     ? static_cast<G4double>(totalBoundaryScatterBN) / totalPathLengthUm
                                     : 0.0;
  const G4double muSBoundaryZnS = (totalPathLengthUm > 0.0)
                                      ? static_cast<G4double>(totalBoundaryScatterZnS) / totalPathLengthUm
                                      : 0.0;
  const G4double muSStepTotal = (totalPathLengthUm > 0.0)
                           ? static_cast<G4double>(totalRealScatter) / totalPathLengthUm
                           : 0.0;
  const G4double muSBulk = (totalPathLengthUm > 0.0)
                               ? static_cast<G4double>(totalBulkScatter) / totalPathLengthUm
                               : 0.0;
  const G4double muSBoundary = (totalPathLengthUm > 0.0)
                                   ? static_cast<G4double>(totalBoundaryScatter) / totalPathLengthUm
                                   : 0.0;
  const G4double gParticle = (totalParticleScatter > 0)
                                 ? sumCosThetaParticleScatter / static_cast<G4double>(totalParticleScatter)
                                 : 0.0;
  const G4double gParticleBN = (totalParticleScatterBN > 0)
                                   ? sumCosThetaParticleScatterBN / static_cast<G4double>(totalParticleScatterBN)
                                   : 0.0;
  const G4double gParticleZnS = (totalParticleScatterZnS > 0)
                                    ? sumCosThetaParticleScatterZnS / static_cast<G4double>(totalParticleScatterZnS)
                                    : 0.0;
  const G4double gBoundaryPrimary = (totalBoundaryScatter > 0)
                                        ? sumCosThetaBoundaryScatter / static_cast<G4double>(totalBoundaryScatter)
                                        : 0.0;
  const G4double gBoundaryBN = (totalBoundaryScatterBN > 0)
                                   ? sumCosThetaBoundaryScatterBN / static_cast<G4double>(totalBoundaryScatterBN)
                                   : 0.0;
  const G4double gBoundaryZnS = (totalBoundaryScatterZnS > 0)
                                    ? sumCosThetaBoundaryScatterZnS / static_cast<G4double>(totalBoundaryScatterZnS)
                                    : 0.0;
  const G4double gStepRaw = (totalRealScatter > 0)
                            ? sumCosThetaAllScatter / static_cast<G4double>(totalRealScatter)
                            : 0.0;
  const G4double gBulk = (totalBulkScatter > 0)
                             ? sumCosThetaBulkScatter / static_cast<G4double>(totalBulkScatter)
                             : 0.0;
  const G4double gBoundary = (totalBoundaryScatter > 0)
                                 ? sumCosThetaBoundaryScatter / static_cast<G4double>(totalBoundaryScatter)
                                 : 0.0;
  const G4double muSPrimeParticle = muSParticle * (1.0 - gParticle);
  const G4double muSPrimeParticleBN = muSBN * (1.0 - gParticleBN);
  const G4double muSPrimeParticleZnS = muSZnS * (1.0 - gParticleZnS);
  const G4double muSPrimeBoundaryPrimary = muSBoundaryPrimary * (1.0 - gBoundaryPrimary);
  const G4double muSPrimeBoundaryBN = muSBoundaryBN * (1.0 - gBoundaryBN);
  const G4double muSPrimeBoundaryZnS = muSBoundaryZnS * (1.0 - gBoundaryZnS);
  const G4double muSPrimeStepTotal = muSStepTotal * (1.0 - gStepRaw);
  const G4double muSPrimeBulk = muSBulk * (1.0 - gBulk);
  const G4double muSPrimeBoundary = muSBoundary * (1.0 - gBoundary);

  G4double muSPrimary = muSParticle;
  G4double muSPrimaryBN = muSBN;
  G4double muSPrimaryZnS = muSZnS;
  G4double gPrimary = gParticle;
  G4double gPrimaryBN = gParticleBN;
  G4double gPrimaryZnS = gParticleZnS;
  G4double muSPrimePrimary = muSPrimeParticle;
  G4double muSPrimePrimaryBN = muSPrimeParticleBN;
  G4double muSPrimePrimaryZnS = muSPrimeParticleZnS;

  if (scatterMetric == "boundary_deflection")
  {
    muSPrimary = muSBoundaryPrimary;
    muSPrimaryBN = muSBoundaryBN;
    muSPrimaryZnS = muSBoundaryZnS;
    gPrimary = gBoundaryPrimary;
    gPrimaryBN = gBoundaryBN;
    gPrimaryZnS = gBoundaryZnS;
    muSPrimePrimary = muSPrimeBoundaryPrimary;
    muSPrimePrimaryBN = muSPrimeBoundaryBN;
    muSPrimePrimaryZnS = muSPrimeBoundaryZnS;
  }
  else if (scatterMetric == "step_angle_threshold")
  {
    muSPrimary = muSStepTotal;
    muSPrimaryBN = 0.0;
    muSPrimaryZnS = 0.0;
    gPrimary = gStepRaw;
    gPrimaryBN = 0.0;
    gPrimaryZnS = 0.0;
    muSPrimePrimary = muSPrimeStepTotal;
    muSPrimePrimaryBN = 0.0;
    muSPrimePrimaryZnS = 0.0;
  }

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
      << "total_particle_scatter,"
      << "mean_num_particle_scatter,"
      << "total_particle_scatter_BN,"
      << "total_particle_scatter_ZnS,"
      << "total_real_scatter,"
      << "mean_num_real_scatter,"
      << "total_bulk_scatter,"
      << "mean_num_bulk_scatter,"
      << "total_boundary_scatter,"
      << "mean_num_boundary_scatter,"
      << "total_reentry,"
      << "mean_num_reentry,"
      << "total_reentry_BN,"
      << "total_reentry_ZnS,"
      << "total_reentry_matrix,"
      << "mu_a_raw_per_um,"
      << "mu_s_raw_per_um,"
      << "mu_s_particle_raw_per_um,"
      << "mu_s_boundary_primary_raw_per_um,"
      << "mu_s_raw_BN_per_um,"
      << "mu_s_raw_ZnS_per_um,"
      << "mu_s_step_total_raw_per_um,"
      << "mu_s_bulk_raw_per_um,"
      << "mu_s_boundary_raw_per_um,"
      << "g_raw,"
      << "g_particle_raw,"
      << "g_boundary_primary_raw,"
      << "g_raw_BN,"
      << "g_raw_ZnS,"
      << "g_step_total_raw,"
      << "g_bulk_raw,"
      << "g_boundary_raw,"
      << "mu_s_prime_raw_per_um,"
      << "mu_s_prime_particle_raw_per_um,"
      << "mu_s_prime_boundary_primary_raw_per_um,"
      << "mu_s_prime_raw_BN_per_um,"
      << "mu_s_prime_raw_ZnS_per_um,"
      << "mu_s_prime_step_total_raw_per_um,"
      << "mu_s_prime_bulk_raw_per_um,"
      << "mu_s_prime_boundary_raw_per_um,"
      << "optical_params_provided,"
      << "scatter_metric,"
      << "target_primary_scatter,"
      << "matrix_n,"
      << "matrix_abs_um,"
      << "bn_n,"
      << "bn_abs_um,"
      << "zns_n,"
      << "zns_abs_um,"
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
      << totalParticleScatter << ","
      << static_cast<G4double>(totalParticleScatter) / nPhotonsD << ","
      << totalParticleScatterBN << ","
      << totalParticleScatterZnS << ","
      << totalRealScatter << ","
      << static_cast<G4double>(totalRealScatter) / nPhotonsD << ","
      << totalBulkScatter << ","
      << static_cast<G4double>(totalBulkScatter) / nPhotonsD << ","
      << totalBoundaryScatter << ","
      << static_cast<G4double>(totalBoundaryScatter) / nPhotonsD << ","
      << totalReentry << ","
      << static_cast<G4double>(totalReentry) / nPhotonsD << ","
      << totalReentryBN << ","
      << totalReentryZnS << ","
      << totalReentryMatrix << ","
      << muA << ","
      << muSPrimary << ","
      << muSParticle << ","
      << muSBoundaryPrimary << ","
      << muSPrimaryBN << ","
      << muSPrimaryZnS << ","
      << muSStepTotal << ","
      << muSBulk << ","
      << muSBoundary << ","
      << gPrimary << ","
      << gParticle << ","
      << gBoundaryPrimary << ","
      << gPrimaryBN << ","
      << gPrimaryZnS << ","
      << gStepRaw << ","
      << gBulk << ","
      << gBoundary << ","
      << muSPrimePrimary << ","
      << muSPrimeParticle << ","
      << muSPrimeBoundaryPrimary << ","
      << muSPrimePrimaryBN << ","
      << muSPrimePrimaryZnS << ","
      << muSPrimeStepTotal << ","
      << muSPrimeBulk << ","
      << muSPrimeBoundary << ","
      << ((fConfig && fConfig->opticalParamsProvided) ? 1 : 0) << ","
      << CsvQuote(fConfig ? fConfig->stageD_scatter_metric : "") << ","
      << (fConfig ? fConfig->stageD_target_primary_scatter : 0) << ","
      << (fConfig ? fConfig->opticalMatrixRIndex : 0.0) << ","
      << (fConfig ? fConfig->opticalMatrixAbsLengthUm : 0.0) << ","
      << (fConfig ? fConfig->opticalBnRIndex : 0.0) << ","
      << (fConfig ? fConfig->opticalBnAbsLengthUm : 0.0) << ","
      << (fConfig ? fConfig->opticalZnsRIndex : 0.0) << ","
      << (fConfig ? fConfig->opticalZnsAbsLengthUm : 0.0) << ","
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
