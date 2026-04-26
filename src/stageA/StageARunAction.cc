#include "StageARunAction.hh"

#include "AnalysisConfig.hh"
#include "DetectorConstruction.hh"
#include "G4RunManager.hh"

#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

#include <fstream>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <cmath>
#include <iomanip>
#include <filesystem>

namespace
{
    void EnsureDirectoryOrThrow(const char *path, const char *where)
    {
        if (mkdir(path, 0777) != 0)
        {
            if (errno != EEXIST)
            {
                std::ostringstream oss;
                oss << "Failed to create directory: " << path
                    << " , errno = " << errno
                    << " , message = " << std::strerror(errno);
                G4Exception(where, "BNZS_A_RUN_DIR", FatalException, oss.str().c_str());
            }
        }
    }

    bool FileExists(const std::string &path)
    {
        std::ifstream fin(path.c_str());
        return fin.good();
    }

    std::string WeightPartToTagString(double v)
    {
        const double rounded = std::round(v);
        if (std::fabs(v - rounded) < 1.0e-9)
        {
            std::ostringstream oss;
            oss << static_cast<long long>(rounded);
            return oss.str();
        }

        std::ostringstream oss;
        oss << v;
        return oss.str();
    }

    std::string CsvQuote(const std::string &s)
    {
        std::string out = "\"";
        for (char c : s)
        {
            if (c == '"')
            {
                out += "\"\"";
            }
            else
            {
                out += c;
            }
        }
        out += "\"";
        return out;
    }

} // namespace

StageARunAction::StageARunAction(AnalysisConfig *config)
    : G4UserRunAction(),
      fConfig(config),
      fIncidentCount(0),
      fCaptureCount(0),
      fTotalTrackLength(0.0),
      fSummaryCsvPath(""),
      fWallStart(),
      fProgressPrintEvery(1000),
      fLastPrintedIncidentCount(0)
{
    if (fConfig == nullptr)
    {
        G4Exception("StageARunAction::StageARunAction",
                    "BNZS_A_RUN_001", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }
}

StageARunAction::~StageARunAction() = default;

void StageARunAction::BeginOfRunAction(const G4Run *run)
{
    ResetAccumulators();

    fWallStart = std::chrono::steady_clock::now();
    fLastPrintedIncidentCount = 0;
    fProgressPrintEvery = 10000; // 先固定成每 100 个 incident 打印一次

    fSummaryCsvPath = BuildSummaryCsvPath();
    EnsureSummaryCsvReady();

    if (run != nullptr)
    {
        G4cout << "[StageARunAction] Begin run: runID=" << run->GetRunID()
               << " mode=" << AnalysisConfig::RunModeName(fConfig->runMode)
               << " patchXY=" << fConfig->patchXY_um << " um"
               << " microThickness=" << fConfig->microThickness_um << " um"
               << G4endl;
    }
    else
    {
        G4cout << "[StageARunAction] Begin run"
               << " mode=" << AnalysisConfig::RunModeName(fConfig->runMode)
               << " patchXY=" << fConfig->patchXY_um << " um"
               << " microThickness=" << fConfig->microThickness_um << " um"
               << G4endl;
    }

    G4cout << "[StageARunAction] Summary CSV: " << fSummaryCsvPath << G4endl;
    G4cout << "[StageARunAction] Progress print every "
           << fProgressPrintEvery << " incident neutrons" << G4endl;
}

void StageARunAction::EndOfRunAction(const G4Run *run)
{
    ForcePrintProgress("final");
    const G4double sigmaEff = GetSigmaEff();

    G4cout << G4endl;
    G4cout << "================ Stage A Summary ================" << G4endl;
    if (run != nullptr)
    {
        G4cout << "Run ID                   : " << run->GetRunID() << G4endl;
    }
    G4cout << "Run mode                 : "
           << AnalysisConfig::RunModeName(fConfig->runMode) << G4endl;
    G4cout << "Patch XY                 : " << fConfig->patchXY_um << " um" << G4endl;
    G4cout << "Micro thickness          : " << fConfig->microThickness_um << " um" << G4endl;
    G4cout << "Incident neutrons        : " << fIncidentCount << G4endl;
    G4cout << "Capture count            : " << fCaptureCount << G4endl;
    G4cout << "Total track length       : " << (fTotalTrackLength / um) << " um" << G4endl;

    if (fTotalTrackLength > 0.0)
    {
        G4cout << "Sigma_eff                : " << (sigmaEff * um) << " 1/um" << G4endl;
        G4cout << "Sigma_eff                : " << (sigmaEff * cm) << " 1/cm" << G4endl;
    }
    else
    {
        G4cout << "Sigma_eff                : undefined (total track length <= 0)" << G4endl;
    }

    if (fIncidentCount > 0)
    {
        G4cout << "Capture probability/event: "
               << static_cast<G4double>(fCaptureCount) / static_cast<G4double>(fIncidentCount)
               << G4endl;
    }
    else
    {
        G4cout << "Capture probability/event: undefined (incident count <= 0)" << G4endl;
    }

    G4cout << "CSV written to           : " << fSummaryCsvPath << G4endl;
    G4cout << "=================================================" << G4endl;
    G4cout << G4endl;

    AppendSummaryRow(run);
}

void StageARunAction::AddIncident(G4int n)
{
    if (n > 0)
    {
        fIncidentCount += n;
    }
}

void StageARunAction::AddCapture(G4int n)
{
    if (n > 0)
    {
        fCaptureCount += n;
    }
}

void StageARunAction::AddTrackLength(G4double len)
{
    if (len > 0.0)
    {
        fTotalTrackLength += len;
    }
}

G4int StageARunAction::GetIncidentCount() const
{
    return fIncidentCount;
}

G4int StageARunAction::GetCaptureCount() const
{
    return fCaptureCount;
}

G4double StageARunAction::GetTotalTrackLength() const
{
    return fTotalTrackLength;
}

G4double StageARunAction::GetSigmaEff() const
{
    if (fTotalTrackLength <= 0.0)
    {
        return 0.0;
    }

    return static_cast<G4double>(fCaptureCount) / fTotalTrackLength;
}

void StageARunAction::ResetAccumulators()
{
    fIncidentCount = 0;
    fCaptureCount = 0;
    fTotalTrackLength = 0.0;
}

void StageARunAction::MaybePrintProgress(const char *reason)
{
    if (fProgressPrintEvery <= 0)
    {
        return;
    }

    if (fIncidentCount < fProgressPrintEvery)
    {
        return;
    }

    const G4int currentBucket = fIncidentCount / fProgressPrintEvery;
    const G4int lastBucket = fLastPrintedIncidentCount / fProgressPrintEvery;

    if (currentBucket <= lastBucket)
    {
        return;
    }

    fLastPrintedIncidentCount = fIncidentCount;
    ForcePrintProgress(reason);
}

void StageARunAction::ForcePrintProgress(const char *reason) const
{
    const auto now = std::chrono::steady_clock::now();
    const double elapsedSec =
        std::chrono::duration_cast<std::chrono::duration<double>>(now - fWallStart).count();

    const G4double sigmaEff = GetSigmaEff();

    G4cout << "[StageA-progress]"
           << " reason=" << (reason ? reason : "unknown")
           << " incident=" << fIncidentCount
           << " capture=" << fCaptureCount
           << " track_um=" << std::fixed << std::setprecision(3)
           << (fTotalTrackLength / um)
           << " sigma_eff_per_um=" << (sigmaEff * um)
           << " elapsed_s=" << elapsedSec
           << G4endl;
}

void StageARunAction::EnsureOutputDirectories() const
{
    namespace fs = std::filesystem;

    const fs::path outputDir = fs::path("..") / "Output";
    const fs::path stageADir = outputDir / "stageA";
    const fs::path ratioDir = fs::path(BuildStageARatioDirectory());

    std::error_code ec;

    fs::create_directories(outputDir, ec);
    if (ec)
    {
        G4Exception("StageARunAction::EnsureOutputDirectories",
                    "BNZS_A_RUN_DIR", FatalException,
                    ("Failed to create directory: " + outputDir.string() +
                     " , message = " + ec.message())
                        .c_str());
        return;
    }

    ec.clear();
    fs::create_directories(stageADir, ec);
    if (ec)
    {
        G4Exception("StageARunAction::EnsureOutputDirectories",
                    "BNZS_A_RUN_DIR", FatalException,
                    ("Failed to create directory: " + stageADir.string() +
                     " , message = " + ec.message())
                        .c_str());
        return;
    }

    ec.clear();
    fs::create_directories(ratioDir, ec);
    if (ec)
    {
        G4Exception("StageARunAction::EnsureOutputDirectories",
                    "BNZS_A_RUN_DIR", FatalException,
                    ("Failed to create directory: " + ratioDir.string() +
                     " , message = " + ec.message())
                        .c_str());
        return;
    }
}

void StageARunAction::EnsureSummaryCsvReady()
{
    EnsureOutputDirectories();

    if (!FileExists(fSummaryCsvPath))
    {
        std::ofstream fout(fSummaryCsvPath.c_str(), std::ios::out);
        if (!fout)
        {
            G4Exception("StageARunAction::EnsureSummaryCsvReady",
                        "BNZS_A_RUN_002", FatalException,
                        ("Cannot open summary CSV for writing: " + fSummaryCsvPath).c_str());
            return;
        }

        fout << "placement_file,"
             << "placement_basename,"
             << "seedBase,"
             << "run_mode,"
             << "patch_xy_um,"
             << "micro_thickness_um,"
             << "bn_wt,"
             << "zns_wt,"
             << "incident_count,"
             << "capture_count,"
             << "total_track_length_um,"
             << "sigma_eff_per_um,"
             << "sigma_eff_per_cm,"
             << "capture_probability"
             << "\n";
    }
}

std::string StageARunAction::BuildRatioTag() const
{
    return WeightPartToTagString(fConfig->bnWt) + "-" +
           WeightPartToTagString(fConfig->znsWt);
}

std::string StageARunAction::BuildStageARatioDirectory() const
{
    namespace fs = std::filesystem;
    fs::path dir = fs::path("..") / "Output" / "stageA" / BuildRatioTag();
    return dir.string();
}

std::string StageARunAction::BuildSummaryCsvPath() const
{
    namespace fs = std::filesystem;
    fs::path path = fs::path(BuildStageARatioDirectory()) / "neutron_transport_summary.csv";
    return path.string();
}

void StageARunAction::AppendSummaryRow(const G4Run *run) const
{
    (void)run;

    std::ofstream fout(fSummaryCsvPath.c_str(), std::ios::app);
    if (!fout)
    {
        G4Exception("StageARunAction::AppendSummaryRow",
                    "BNZS_A_RUN_003", FatalException,
                    ("Cannot open summary CSV for appending: " + fSummaryCsvPath).c_str());
        return;
    }

    const G4double sigmaEff = GetSigmaEff();
    const G4double captureProb =
        (fIncidentCount > 0)
            ? static_cast<G4double>(fCaptureCount) / static_cast<G4double>(fIncidentCount)
            : 0.0;

    std::string placementFile = "unknown";
    std::string placementBaseName = "unknown";
    std::string seedBase = "unknown";

    auto *runManager = G4RunManager::GetRunManager();
    if (runManager != nullptr)
    {
        const auto *detector =
            static_cast<const DetectorConstruction *>(
                runManager->GetUserDetectorConstruction());

        if (detector != nullptr)
        {
            placementFile = detector->GetLoadedPlacementFile();
            seedBase = detector->GetLoadedPlacementSeedBase();
            if (!placementFile.empty())
            {
                placementBaseName =
                    std::filesystem::path(placementFile).filename().string();
            }
            if (seedBase.empty())
            {
                seedBase = "unknown";
            }
        }
    }

    fout << CsvQuote(placementFile) << ","
         << CsvQuote(placementBaseName) << ","
         << seedBase << ","
         << AnalysisConfig::RunModeName(fConfig->runMode) << ","
         << fConfig->patchXY_um << ","
         << fConfig->microThickness_um << ","
         << fConfig->bnWt << ","
         << fConfig->znsWt << ","
         << fIncidentCount << ","
         << fCaptureCount << ","
         << (fTotalTrackLength / um) << ","
         << (sigmaEff * um) << ","
         << (sigmaEff * cm) << ","
         << captureProb
         << "\n";
}
