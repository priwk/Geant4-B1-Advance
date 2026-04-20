#include "PrimaryGeneratorAction.hh"

#include "DetectorConstruction.hh"

#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "G4Exception.hh"
#include "G4ExceptionSeverity.hh"

#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
// #include <dirent.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numeric>

// --------------------------------------------------------------------
// small helpers in anonymous namespace
namespace
{
  std::vector<std::string> SplitFlexible(const std::string &line)
  {
    std::string s = line;
    for (char &c : s)
    {
      if (c == ',')
        c = '\t';
    }

    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '\t'))
    {
      if (!token.empty() && token.back() == '\r')
        token.pop_back();
      tokens.push_back(token);
    }
    return tokens;
  }

  G4ThreeVector RandomUnitVector()
  {
    const G4double cosTheta = 2.0 * G4UniformRand() - 1.0;
    const G4double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
    const G4double phi = twopi * G4UniformRand();

    return G4ThreeVector(
        sinTheta * std::cos(phi),
        sinTheta * std::sin(phi),
        cosTheta);
  }

  G4double ClampValue(G4double x, G4double lo, G4double hi)
  {
    if (x < lo)
      return lo;
    if (x > hi)
      return hi;
    return x;
  }

  G4bool EndsWith(const std::string &s, const std::string &suffix)
  {
    if (s.size() < suffix.size())
      return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  std::filesystem::path MakeInputRootDirectoryPath()
  {
    return std::filesystem::current_path().parent_path() / "Input";
  }

  std::filesystem::path MakeCaptureInputRootDirectoryPath()
  {
    return MakeInputRootDirectoryPath() / "neutron_capture_positions";
  }

  G4bool TryParseRatioFolderName(const std::string &name,
                                 G4double &bnWt,
                                 G4double &znsWt)
  {
    const std::size_t dashPos = name.find('-');
    if (dashPos == std::string::npos)
      return false;

    const std::string lhs = name.substr(0, dashPos);
    const std::string rhs = name.substr(dashPos + 1);

    if (lhs.empty() || rhs.empty())
      return false;

    try
    {
      bnWt = std::stod(lhs);
      znsWt = std::stod(rhs);
    }
    catch (...)
    {
      return false;
    }

    return (bnWt > 0.0 && znsWt > 0.0);
  }

  std::vector<std::string> CollectMatchingCsvFilesInDirectory(const std::filesystem::path &dir)
  {
    namespace fs = std::filesystem;
    std::vector<std::string> matches;

    if (!fs::exists(dir) || !fs::is_directory(dir))
      return matches;

    for (const auto &entry : fs::directory_iterator(dir))
    {
      if (!entry.is_regular_file())
        continue;

      const std::string name = entry.path().filename().string();
      if (EndsWith(name, "_neutron_capture_positions.csv"))
      {
        matches.push_back(entry.path().string());
      }
    }

    std::sort(matches.begin(), matches.end());
    return matches;
  }
}

// --------------------------------------------------------------------

PrimaryGeneratorAction::PrimaryGeneratorAction()
    : G4VUserPrimaryGeneratorAction(),
      fParticleGun(nullptr),
      fInputFiles(),
      fCurrentFileIndex(0),
      fCurrentInputStream(),
      fCurrentInputFile(""),
      fFirstRecordForGeometry(),
      fHasFirstRecordForGeometry(false),
      fNoMoreInput(false),
      fTotalStreamedRecords(0),
      fCurrentRecord(),
      fCurrentLocalCapturePosition(),
      fCurrentSelectedBNCenter(),
      fCurrentSurfaceMode(""),
      fCurrentTargetLocalZ(0.0),
      fCurrentUsedLocalZ(0.0)
{
  fParticleGun = new G4ParticleGun(1);

  InitializeInputStreaming();
  ConfigureDetectorFromInput();

  // temporary default, will be overwritten event-by-event
  auto *particleTable = G4ParticleTable::GetParticleTable();
  auto *alpha = particleTable->FindParticle("alpha");
  fParticleGun->SetParticleDefinition(alpha);
  fParticleGun->SetParticleEnergy(1.0 * MeV);
  fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
  fParticleGun->SetParticlePosition(G4ThreeVector());
}

// --------------------------------------------------------------------

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fParticleGun;
}

// --------------------------------------------------------------------

std::vector<std::string> PrimaryGeneratorAction::FindInputCsvFiles() const
{
  // Priority 1: environment variable -> single explicit file
  const char *envPath = std::getenv("BNZS_INPUT_CSV");
  if (envPath && std::string(envPath).size() > 0)
  {
    return {std::string(envPath)};
  }

  namespace fs = std::filesystem;

  // Priority 2: preferred new layout
  // ../Input/neutron_capture_positions/<ratio>/*.csv
  const fs::path captureRoot = MakeCaptureInputRootDirectoryPath();

  if (fs::exists(captureRoot) && fs::is_directory(captureRoot))
  {
    std::vector<fs::path> candidateRatioDirs;

    for (const auto &entry : fs::directory_iterator(captureRoot))
    {
      if (!entry.is_directory())
        continue;

      G4double bnWt = 0.0, znsWt = 0.0;
      const std::string dirName = entry.path().filename().string();

      if (!TryParseRatioFolderName(dirName, bnWt, znsWt))
        continue;

      auto matches = CollectMatchingCsvFilesInDirectory(entry.path());
      if (!matches.empty())
      {
        candidateRatioDirs.push_back(entry.path());
      }
    }

    if (candidateRatioDirs.size() == 1)
    {
      return CollectMatchingCsvFilesInDirectory(candidateRatioDirs.front());
    }

    if (candidateRatioDirs.size() > 1)
    {
      std::ostringstream oss;
      oss << "Multiple non-empty ratio folders found under "
          << captureRoot.string()
          << ". Keep only one ratio folder for one run, or use BNZS_INPUT_CSV.";
      G4Exception("PrimaryGeneratorAction::FindInputCsvFiles",
                  "BNZS001", FatalException,
                  oss.str().c_str());
      return {};
    }
  }

  // Priority 3: backward compatibility
  // old flat layout: ../Input/*_neutron_capture_positions.csv
  const fs::path inputDir = MakeInputRootDirectoryPath();
  auto matches = CollectMatchingCsvFilesInDirectory(inputDir);

  if (matches.empty())
  {
    G4Exception("PrimaryGeneratorAction::FindInputCsvFiles",
                "BNZS002", FatalException,
                ("No *_neutron_capture_positions.csv found in either " + MakeCaptureInputRootDirectoryPath().string() + " or " + inputDir.string()).c_str());
    return {};
  }

  return matches;
}

// --------------------------------------------------------------------

G4bool PrimaryGeneratorAction::ParseOneRecordLine(const std::string &line, CaptureRecord &rec) const
{
  const auto tokens = SplitFlexible(line);
  if (tokens.size() < 9)
    return false;

  try
  {
    rec.eventID = std::stoi(tokens[0]);
    rec.thickness_um = std::stod(tokens[1]);
    rec.bn_wt = std::stod(tokens[2]);
    rec.zns_wt = std::stod(tokens[3]);
    rec.capture_x_um = std::stod(tokens[4]);
    rec.capture_y_um = std::stod(tokens[5]);
    rec.corr_x_um = std::stod(tokens[6]);
    rec.corr_y_um = std::stod(tokens[7]);
    rec.depth_um = std::stod(tokens[8]);
  }
  catch (...)
  {
    return false;
  }

  return true;
}

// --------------------------------------------------------------------

G4bool PrimaryGeneratorAction::ReadFirstValidRecordFromFile(
    const std::string &path, CaptureRecord &rec) const
{
  std::ifstream fin(path.c_str());
  if (!fin)
    return false;

  std::string line;

  // skip header
  if (!std::getline(fin, line))
    return false;

  while (std::getline(fin, line))
  {
    if (line.empty())
      continue;

    if (ParseOneRecordLine(line, rec))
    {
      return true;
    }
  }

  return false;
}

G4bool PrimaryGeneratorAction::OpenNextInputFile()
{
  if (fCurrentInputStream.is_open())
  {
    fCurrentInputStream.close();
  }

  while (fCurrentFileIndex < fInputFiles.size())
  {
    fCurrentInputFile = fInputFiles[fCurrentFileIndex++];
    fCurrentInputStream.open(fCurrentInputFile.c_str());

    if (!fCurrentInputStream)
    {
      continue;
    }

    std::string header;
    if (!std::getline(fCurrentInputStream, header))
    {
      fCurrentInputStream.close();
      continue;
    }

    G4cout << "[PrimaryGeneratorAction] Open input CSV: "
           << fCurrentInputFile << G4endl;
    return true;
  }

  fNoMoreInput = true;
  return false;
}

G4bool PrimaryGeneratorAction::ReadNextRecord(CaptureRecord &rec)
{
  std::string line;

  while (true)
  {
    if (!fCurrentInputStream.is_open())
    {
      if (!OpenNextInputFile())
      {
        return false;
      }
    }

    while (std::getline(fCurrentInputStream, line))
    {
      if (line.empty())
        continue;

      if (ParseOneRecordLine(line, rec))
      {
        ++fTotalStreamedRecords;
        return true;
      }
    }

    fCurrentInputStream.close();
  }
}

void PrimaryGeneratorAction::InitializeInputStreaming()
{
  fInputFiles = FindInputCsvFiles();
  fCurrentFileIndex = 0;
  fNoMoreInput = false;
  fTotalStreamedRecords = 0;
  fHasFirstRecordForGeometry = false;

  if (fInputFiles.empty())
  {
    G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                "BNZS003", FatalException,
                "No input CSV files found.");
    return;
  }

  CaptureRecord refRec;
  if (!ReadFirstValidRecordFromFile(fInputFiles.front(), refRec))
  {
    G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                "BNZS004", FatalException,
                ("Failed to read first valid record from: " + fInputFiles.front()).c_str());
    return;
  }

  fFirstRecordForGeometry = refRec;
  fHasFirstRecordForGeometry = true;

  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double localT_um = det ? det->GetEffectiveLocalThickness() / um : 30.0;

  for (const auto &path : fInputFiles)
  {
    CaptureRecord rec;
    if (!ReadFirstValidRecordFromFile(path, rec))
    {
      G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                  "BNZS005", FatalException,
                  ("Failed to read first valid record from: " + path).c_str());
      return;
    }

    if (std::abs(rec.bn_wt - refRec.bn_wt) > 1e-12 ||
        std::abs(rec.zns_wt - refRec.zns_wt) > 1e-12)
    {
      G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                  "BNZS006", FatalException,
                  ("Mixed BN:ZnS ratios are not allowed across input files: " + path).c_str());
      return;
    }

    if (rec.thickness_um <= localT_um)
    {
      G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                  "BNZS007", FatalException,
                  ("Found thickness <= local micro thickness in: " + path).c_str());
      return;
    }
  }

  if (!OpenNextInputFile())
  {
    G4Exception("PrimaryGeneratorAction::InitializeInputStreaming",
                "BNZS008", FatalException,
                "Failed to open the first input CSV.");
    return;
  }

  G4cout
      << "\n[PrimaryGeneratorAction] Streaming input enabled"
      << "\n  files = " << fInputFiles.size()
      << "\n  fixed BN:ZnS wt = " << refRec.bn_wt << " : " << refRec.zns_wt
      << "\n  local geometry thickness = " << localT_um << " um"
      << G4endl;
}

// --------------------------------------------------------------------

void PrimaryGeneratorAction::ConfigureDetectorFromInput()
{
  const auto *detConst = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  auto *det = const_cast<DetectorConstruction *>(detConst);

  if (!det || !fHasFirstRecordForGeometry)
  {
    G4Exception("PrimaryGeneratorAction::ConfigureDetectorFromInput",
                "BNZS009", FatalException,
                "DetectorConstruction is not available or first input record missing.");
    return;
  }

  // Keep microstructure geometry fixed.
  // Only set composition once from the first input record.
  det->SetWeightRatio(fFirstRecordForGeometry.bn_wt,
                      fFirstRecordForGeometry.zns_wt);
}

// --------------------------------------------------------------------

std::string PrimaryGeneratorAction::DetermineSurfaceMode(const CaptureRecord &rec) const
{
  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double screenT = rec.thickness_um;
  const G4double localT = det->GetEffectiveLocalThickness() / um;
  const G4double surfaceCut = 0.5 * localT;

  if (rec.depth_um <= surfaceCut)
  {
    return "front_surface";
  }

  if ((screenT - rec.depth_um) <= surfaceCut)
  {
    return "back_surface";
  }

  return "bulk";
}

// --------------------------------------------------------------------

G4double PrimaryGeneratorAction::DetermineTargetLocalZ(
    const CaptureRecord &rec, const std::string &mode) const
{
  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double localT = det->GetEffectiveLocalThickness();

  if (mode == "front_surface")
  {
    return +0.5 * localT - rec.depth_um * um;
  }

  if (mode == "back_surface")
  {
    return -0.5 * localT + (rec.thickness_um - rec.depth_um) * um;
  }

  // bulk: keep the relative ordering of depth, but clamp it away from both surfaces
  const G4double zRaw = +0.5 * localT - rec.depth_um * um;

  // choose a conservative "do not get too close to z-surfaces" margin
  const G4double zSafeMargin = 7.0 * um; // can tune: 5~8 um
  const G4double zMinSafe = -0.5 * localT + zSafeMargin;
  const G4double zMaxSafe = +0.5 * localT - zSafeMargin;

  return ClampValue(zRaw, zMinSafe, zMaxSafe);
}

// --------------------------------------------------------------------

G4bool PrimaryGeneratorAction::SelectBNSphereForTargetZ(
    G4double targetZ,
    const std::string &mode,
    G4ThreeVector &chosenCenter,
    G4double &usedZ,
    G4bool &usedFallback) const
{
  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double R = det->GetBNRadius();
  const G4double eps = 1.0e-6 * um;

  // first try safe BN centers
  auto trySelect = [&](const std::vector<G4ThreeVector> &centers,
                       G4ThreeVector &centerOut,
                       G4double &zOut,
                       G4bool &fallbackOut) -> G4bool
  {
    std::vector<G4int> candidateIdx;
    std::vector<G4double> weights;
    std::vector<G4double> rxyList;

    G4double maxRxy = 0.0;

    for (G4int i = 0; i < static_cast<G4int>(centers.size()); ++i)
    {
      const G4double dz = targetZ - centers[i].z();
      if (std::abs(dz) < R)
      {
        const G4double baseW = pi * (R * R - dz * dz);
        if (baseW > 0.0)
        {
          const G4double rxy =
              std::sqrt(centers[i].x() * centers[i].x() +
                        centers[i].y() * centers[i].y());

          candidateIdx.push_back(i);
          weights.push_back(baseW);
          rxyList.push_back(rxy);

          if (rxy > maxRxy)
            maxRxy = rxy;
        }
      }
    }

    if (!candidateIdx.empty())
    {
      // For bulk events, favor BN spheres closer to XY center
      if (mode == "bulk" && maxRxy > 0.0)
      {
        for (G4int k = 0; k < static_cast<G4int>(weights.size()); ++k)
        {
          const G4double x = rxyList[k] / maxRxy;  // 0 at center, 1 at outermost candidate
          const G4double centrality = 1.0 - x * x; // larger near center
          const G4double centralBoost = 0.25 + 0.75 * centrality;
          weights[k] *= centralBoost;
        }
      }

      const G4double sumW = std::accumulate(weights.begin(), weights.end(), 0.0);
      G4double pick = G4UniformRand() * sumW;

      for (G4int k = 0; k < static_cast<G4int>(candidateIdx.size()); ++k)
      {
        pick -= weights[k];
        if (pick <= 0.0 || k == static_cast<G4int>(candidateIdx.size()) - 1)
        {
          centerOut = centers[candidateIdx[k]];
          zOut = targetZ;
          fallbackOut = false;
          return true;
        }
      }
    }

    if (centers.empty())
      return false;

    // fallback: nearest in z, and for bulk break ties toward smaller rxy
    G4double bestScore = DBL_MAX;
    G4int bestIdx = -1;

    for (G4int i = 0; i < static_cast<G4int>(centers.size()); ++i)
    {
      const G4double dzAbs = std::abs(targetZ - centers[i].z());
      const G4double rxy =
          std::sqrt(centers[i].x() * centers[i].x() +
                    centers[i].y() * centers[i].y());

      G4double score = dzAbs;
      if (mode == "bulk")
      {
        score += 0.05 * rxy; // light penalty for being far from XY center
      }

      if (score < bestScore)
      {
        bestScore = score;
        bestIdx = i;
      }
    }

    if (bestIdx < 0)
      return false;

    centerOut = centers[bestIdx];
    zOut = ClampValue(targetZ,
                      centerOut.z() - (R - eps),
                      centerOut.z() + (R - eps));
    fallbackOut = true;
    return true;
  };

  // first safe BN
  if (trySelect(det->GetSafeBNCenters(), chosenCenter, usedZ, usedFallback))
  {
    return true;
  }

  // then all BN
  if (trySelect(det->GetPlacedBNCenters(), chosenCenter, usedZ, usedFallback))
  {
    return true;
  }

  return false;
}

// --------------------------------------------------------------------

G4ThreeVector PrimaryGeneratorAction::SamplePointInSphereSlice(
    const G4ThreeVector &center,
    G4double zSlice,
    G4double sphereRadius) const
{
  const G4double dz = zSlice - center.z();
  const G4double diskR = std::sqrt(std::max(0.0, sphereRadius * sphereRadius - dz * dz));

  const G4double r = diskR * std::sqrt(G4UniformRand());
  const G4double phi = twopi * G4UniformRand();

  const G4double x = center.x() + r * std::cos(phi);
  const G4double y = center.y() + r * std::sin(phi);

  return G4ThreeVector(x, y, zSlice);
}

// --------------------------------------------------------------------

void PrimaryGeneratorAction::GenerateReactionProducts(
    G4Event *event,
    const G4ThreeVector &position,
    G4bool useGroundStateBranch) const
{
  auto *particleTable = G4ParticleTable::GetParticleTable();
  auto *ionTable = G4IonTable::GetIonTable();

  auto *alphaDef = particleTable->FindParticle("alpha");
  auto *li7Def = ionTable->GetIon(3, 7, 0.0);

  if (!alphaDef || !li7Def)
  {
    G4Exception("PrimaryGeneratorAction::GenerateReactionProducts",
                "BNZS007", FatalException,
                "Failed to obtain alpha or Li7 particle definition.");
    return;
  }

  G4double eAlpha = 0.0;
  G4double eLi7 = 0.0;

  // 10B(n,alpha)7Li
  // excited branch ~93.7%: alpha 1.47 MeV, Li7 0.84 MeV (+478 keV gamma, not emitted here)
  // ground branch  ~6.3% : alpha 1.78 MeV, Li7 1.01 MeV
  if (useGroundStateBranch)
  {
    eAlpha = 1.776 * MeV;
    eLi7 = 1.013 * MeV;
  }
  else
  {
    eAlpha = 1.470 * MeV;
    eLi7 = 0.840 * MeV;
  }

  const G4ThreeVector dir = RandomUnitVector();

  // alpha
  fParticleGun->SetParticleDefinition(alphaDef);
  fParticleGun->SetParticleEnergy(eAlpha);
  fParticleGun->SetParticlePosition(position);
  fParticleGun->SetParticleMomentumDirection(dir);
  fParticleGun->GeneratePrimaryVertex(event);

  // Li7 in opposite direction
  fParticleGun->SetParticleDefinition(li7Def);
  fParticleGun->SetParticleEnergy(eLi7);
  fParticleGun->SetParticlePosition(position);
  fParticleGun->SetParticleMomentumDirection(-dir);
  fParticleGun->GeneratePrimaryVertex(event);
}

// --------------------------------------------------------------------

void PrimaryGeneratorAction::GeneratePrimaries(G4Event *event)
{
  const G4int geantEventID = event->GetEventID();

  CaptureRecord rec;
  if (!ReadNextRecord(rec))
  {
    G4cout
        << "[PrimaryGeneratorAction] No more input capture records. "
        << "Aborting run at Geant4 event " << geantEventID << G4endl;

    G4RunManager::GetRunManager()->AbortRun(true);
    return;
  }

  fCurrentRecord = rec;
  fCurrentSurfaceMode = DetermineSurfaceMode(fCurrentRecord);
  fCurrentTargetLocalZ = DetermineTargetLocalZ(fCurrentRecord, fCurrentSurfaceMode);

  // small z jitter to avoid over-discrete slicing
  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double localT = det->GetEffectiveLocalThickness();
  const G4double zJitter = 0.5 * um; // start small: 0.5 um

  // do NOT jitter true surface events too much
  if (fCurrentSurfaceMode == "bulk")
  {
    fCurrentTargetLocalZ += (2.0 * G4UniformRand() - 1.0) * zJitter;
    fCurrentTargetLocalZ = ClampValue(
        fCurrentTargetLocalZ,
        -0.5 * localT + 1.0 * um,
        +0.5 * localT - 1.0 * um);
  }

  G4ThreeVector chosenCenter;
  G4double usedZ = 0.0;
  G4bool usedFallback = false;

  if (!SelectBNSphereForTargetZ(
          fCurrentTargetLocalZ,
          fCurrentSurfaceMode,
          chosenCenter,
          usedZ,
          usedFallback))
  {
    G4Exception("PrimaryGeneratorAction::GeneratePrimaries",
                "BNZS008", FatalException,
                "Failed to find any BN sphere for current capture event.");
    return;
  }

  fCurrentSelectedBNCenter = chosenCenter;
  fCurrentUsedLocalZ = usedZ;
  fCurrentLocalCapturePosition = SamplePointInSphereSlice(
      chosenCenter, usedZ, det->GetBNRadius());

  // choose branch
  const G4bool useGroundStateBranch = (G4UniformRand() < 0.063);

  GenerateReactionProducts(
      event,
      fCurrentLocalCapturePosition,
      useGroundStateBranch);

  if (geantEventID < 5)
  {
    G4cout
        << "\n[PrimaryGeneratorAction] Event " << geantEventID
        << "\n  input eventID      = " << fCurrentRecord.eventID
        << "\n  mode               = " << fCurrentSurfaceMode
        << "\n  target z           = " << fCurrentTargetLocalZ / um << " um"
        << "\n  used z             = " << fCurrentUsedLocalZ / um << " um"
        << "\n  selected BN center = (" << fCurrentSelectedBNCenter.x() / um
        << ", " << fCurrentSelectedBNCenter.y() / um
        << ", " << fCurrentSelectedBNCenter.z() / um << ") um"
        << "\n  capture point      = (" << fCurrentLocalCapturePosition.x() / um
        << ", " << fCurrentLocalCapturePosition.y() / um
        << ", " << fCurrentLocalCapturePosition.z() / um << ") um"
        << "\n  fallback used      = " << (usedFallback ? "yes" : "no")
        << G4endl;
  }
}