#include "StageCOpticalPrimaryGeneratorAction.hh"

#include "AnalysisConfig.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4OpticalPhoton.hh"
#include "G4ParticleDefinition.hh"
#include "G4PhysicalConstants.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

namespace
{
  std::string Trim(const std::string &s)
  {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
      ++b;

    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
      --e;

    return s.substr(b, e - b);
  }

  std::string ToLowerCopy(std::string s)
  {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return s;
  }

  std::vector<std::string> SplitCsvLine(const std::string &line)
  {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
    {
      fields.push_back(Trim(field));
    }
    return fields;
  }

  std::string ResolveSourcePath(const AnalysisConfig *config)
  {
    if (config != nullptr && !config->opticalSourcePath.empty())
      return config->opticalSourcePath;

    const char *envPath = std::getenv("BNZS_OPTICAL_SOURCE_CSV");
    if (envPath != nullptr && std::string(envPath).size() > 0)
      return std::string(envPath);

    envPath = std::getenv("BNZS_STAGEC_SOURCE_CSV");
    if (envPath != nullptr && std::string(envPath).size() > 0)
      return std::string(envPath);

    return "";
  }

  std::string PlacementKey(const std::string &placementFile)
  {
    const std::string trimmed = Trim(placementFile);
    const auto path = std::filesystem::path(trimmed);
    const std::string parent = path.parent_path().filename().string();
    const std::string name = path.filename().string();
    if (!parent.empty() && !name.empty())
      return parent + "/" + name;
    return trimmed;
  }

  std::string WeightPartToTagString(G4double value)
  {
    std::ostringstream os;
    os << value;
    std::string s = os.str();
    std::replace(s.begin(), s.end(), '.', 'p');
    return s;
  }
}

StageCOpticalPrimaryGeneratorAction::StageCOpticalPrimaryGeneratorAction(AnalysisConfig *config)
    : G4VUserPrimaryGeneratorAction(),
      fConfig(config),
      fParticleGun(new G4ParticleGun(1)),
      fSourceCsvPath(ResolveSourcePath(config)),
      fSourceCsv(),
      fHeaderIndex(),
      fCurrentSource(),
      fCurrentPhoton(),
      fHasCurrentSource(false),
      fNextSampleIndex(0),
      fSamplesPerStep(config != nullptr ? std::max(1, config->opticalSamplesPerStep) : 1),
      fSourceSampling(config != nullptr ? config->sourceSampling : "uniformAlongStep"),
      fExpectedPlacementFile(""),
      fLoadedSourceSteps(0),
      fGeneratedPhotons(0),
      fGeneratedPhotonWeight(0.0)
{
  const std::string samplingLower = ToLowerCopy(fSourceSampling);
  if (samplingLower == "midpoint")
  {
    fSourceSampling = "midpoint";
  }
  else
  {
    fSourceSampling = "uniformAlongStep";
  }

  auto *opticalPhoton = G4OpticalPhoton::OpticalPhotonDefinition();
  const G4double energy450nm = h_Planck * c_light / (450.0 * nm);

  fParticleGun->SetParticleDefinition(opticalPhoton);
  fParticleGun->SetParticleEnergy(energy450nm);
  fParticleGun->SetParticleTime(0.0);

  OpenSourceCsv();

  G4cout << "[StageCOpticalPrimary] Initialized"
         << "\n  source CSV          = " << fSourceCsvPath
         << "\n  sourceSampling      = " << fSourceSampling
         << "\n  samples per step    = " << fSamplesPerStep
         << "\n  photon wavelength   = 450 nm"
         << G4endl;
}

StageCOpticalPrimaryGeneratorAction::~StageCOpticalPrimaryGeneratorAction()
{
  if (fSourceCsv.is_open())
    fSourceCsv.close();
  delete fParticleGun;
}

void StageCOpticalPrimaryGeneratorAction::OpenSourceCsv()
{
  if (fSourceCsvPath.empty())
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::OpenSourceCsv",
                "BNZS_C_PRI_001", FatalException,
                "Stage C optical source CSV is not set. Use /cfg/setOpticalSourceCsv or BNZS_OPTICAL_SOURCE_CSV.");
    return;
  }

  fSourceCsv.open(fSourceCsvPath.c_str());
  if (!fSourceCsv)
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::OpenSourceCsv",
                "BNZS_C_PRI_002", FatalException,
                ("Failed to open Stage C optical source CSV: " + fSourceCsvPath).c_str());
    return;
  }

  ReadHeader();
}

void StageCOpticalPrimaryGeneratorAction::ReadHeader()
{
  std::string headerLine;
  if (!std::getline(fSourceCsv, headerLine))
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::ReadHeader",
                "BNZS_C_PRI_003", FatalException,
                "Stage C optical source CSV is empty.");
    return;
  }

  const auto headers = SplitCsvLine(headerLine);
  for (std::size_t i = 0; i < headers.size(); ++i)
  {
    fHeaderIndex[headers[i]] = i;
  }

  const char *required[] = {
      "eventID", "trackID", "stepID", "particle",
      "thickness_um", "bn_wt", "zns_wt", "depth_um", "placement_file",
      "x_pre_um", "y_pre_um", "z_pre_um",
      "x_post_um", "y_post_um", "z_post_um",
      "edep_keV", "visible_edep_keV", "n_photon_step"};

  for (const char *name : required)
  {
    if (fHeaderIndex.find(name) == fHeaderIndex.end())
    {
      G4Exception("StageCOpticalPrimaryGeneratorAction::ReadHeader",
                  "BNZS_C_PRI_004", FatalException,
                  (std::string("Missing required Stage C source column: ") + name).c_str());
      return;
    }
  }
}

std::string StageCOpticalPrimaryGeneratorAction::Field(
    const std::vector<std::string> &fields,
    const std::string &name) const
{
  const auto it = fHeaderIndex.find(name);
  if (it == fHeaderIndex.end() || it->second >= fields.size())
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::Field",
                "BNZS_C_PRI_005", FatalException,
                ("Missing field while parsing Stage C source CSV: " + name).c_str());
    return "";
  }
  return fields[it->second];
}

std::string StageCOpticalPrimaryGeneratorAction::OptionalField(
    const std::vector<std::string> &fields,
    const std::string &name,
    const std::string &fallback) const
{
  const auto it = fHeaderIndex.find(name);
  if (it == fHeaderIndex.end() || it->second >= fields.size())
    return fallback;
  return fields[it->second];
}

G4double StageCOpticalPrimaryGeneratorAction::DoubleField(
    const std::vector<std::string> &fields,
    const std::string &name) const
{
  try
  {
    return std::stod(Field(fields, name));
  }
  catch (...)
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::DoubleField",
                "BNZS_C_PRI_006", FatalException,
                ("Invalid numeric field while parsing Stage C source CSV: " + name).c_str());
    return 0.0;
  }
}

G4double StageCOpticalPrimaryGeneratorAction::OptionalDoubleField(
    const std::vector<std::string> &fields,
    const std::string &name,
    G4double fallback) const
{
  const auto it = fHeaderIndex.find(name);
  if (it == fHeaderIndex.end() || it->second >= fields.size())
    return fallback;
  try
  {
    return std::stod(fields[it->second]);
  }
  catch (...)
  {
    return fallback;
  }
}

G4int StageCOpticalPrimaryGeneratorAction::IntField(
    const std::vector<std::string> &fields,
    const std::string &name) const
{
  try
  {
    return std::stoi(Field(fields, name));
  }
  catch (...)
  {
    G4Exception("StageCOpticalPrimaryGeneratorAction::IntField",
                "BNZS_C_PRI_007", FatalException,
                ("Invalid integer field while parsing Stage C source CSV: " + name).c_str());
    return 0;
  }
}

G4bool StageCOpticalPrimaryGeneratorAction::FillSourceRecord(
    const std::string &line,
    StageCSourceRecord &record) const
{
  const auto fields = SplitCsvLine(line);
  if (fields.empty())
    return false;

  record.eventID = IntField(fields, "eventID");
  record.trackID = IntField(fields, "trackID");
  record.stepID = IntField(fields, "stepID");
  record.particle = Field(fields, "particle");

  record.thickness_um = DoubleField(fields, "thickness_um");
  record.bn_wt = DoubleField(fields, "bn_wt");
  record.zns_wt = DoubleField(fields, "zns_wt");
  record.ratio_label = OptionalField(
      fields,
      "ratio_label",
      WeightPartToTagString(record.bn_wt) + "-" + WeightPartToTagString(record.zns_wt));
  record.depth_um = DoubleField(fields, "depth_um");
  record.capture_depth_um = OptionalDoubleField(fields, "capture_depth_um", record.depth_um);
  record.placement_file = Field(fields, "placement_file");
  record.placement_hash = OptionalField(fields, "placement_hash", PlacementKey(record.placement_file));

  std::ostringstream eventFallback;
  eventFallback << record.ratio_label << "|"
                << record.thickness_um << "|"
                << record.placement_hash << "|"
                << record.eventID;
  record.source_event_uid = OptionalField(fields, "source_event_uid", eventFallback.str());

  std::ostringstream stepFallback;
  stepFallback << record.source_event_uid << "|"
               << record.trackID << "|"
               << record.stepID << "|"
               << record.particle;
  record.source_step_uid = OptionalField(fields, "source_step_uid", stepFallback.str());

  record.xPre = G4ThreeVector(DoubleField(fields, "x_pre_um") * um,
                              DoubleField(fields, "y_pre_um") * um,
                              DoubleField(fields, "z_pre_um") * um);
  record.xPost = G4ThreeVector(DoubleField(fields, "x_post_um") * um,
                               DoubleField(fields, "y_post_um") * um,
                               DoubleField(fields, "z_post_um") * um);

  record.edep_keV = DoubleField(fields, "edep_keV");
  record.visible_edep_keV = DoubleField(fields, "visible_edep_keV");
  record.n_photon_step = DoubleField(fields, "n_photon_step");
  record.step_len_um = OptionalDoubleField(
      fields,
      "step_len_um",
      (record.xPost - record.xPre).mag() / um);

  return true;
}

void StageCOpticalPrimaryGeneratorAction::RequireSinglePlacement(
    const StageCSourceRecord &record)
{
  if (fExpectedPlacementFile.empty())
  {
    fExpectedPlacementFile = record.placement_file;
    return;
  }

  if (PlacementKey(record.placement_file) == PlacementKey(fExpectedPlacementFile))
    return;

  std::ostringstream message;
  message
      << "Stage C optical source CSV contains more than one placement_file.\n"
      << "Each StageC_OpticalRVE run uses one RVE geometry, so split the source CSV "
      << "by placement_file before running.\n"
      << "First placement: " << fExpectedPlacementFile << "\n"
      << "Current placement at source row " << (fLoadedSourceSteps + 1) << ": "
      << record.placement_file << "\n"
      << "Use: python3 stageC_make_zns_sources.py --split-by-placement";

  G4Exception("StageCOpticalPrimaryGeneratorAction::RequireSinglePlacement",
              "BNZS_C_PRI_008", FatalException,
              message.str().c_str());
}

G4bool StageCOpticalPrimaryGeneratorAction::ReadNextSourceRecord(StageCSourceRecord &record)
{
  std::string line;
  while (std::getline(fSourceCsv, line))
  {
    if (line.empty())
      continue;

    if (FillSourceRecord(line, record))
    {
      RequireSinglePlacement(record);
      ++fLoadedSourceSteps;
      return true;
    }
  }

  return false;
}

G4ThreeVector StageCOpticalPrimaryGeneratorAction::SampleSourcePosition(
    const StageCSourceRecord &record) const
{
  const G4double u = (fSourceSampling == "midpoint") ? 0.5 : G4UniformRand();
  return record.xPre + u * (record.xPost - record.xPre);
}

G4ThreeVector StageCOpticalPrimaryGeneratorAction::RandomUnitVector() const
{
  const G4double cosTheta = 2.0 * G4UniformRand() - 1.0;
  const G4double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
  const G4double phi = twopi * G4UniformRand();

  return G4ThreeVector(
      sinTheta * std::cos(phi),
      sinTheta * std::sin(phi),
      cosTheta);
}

G4ThreeVector StageCOpticalPrimaryGeneratorAction::RandomPolarization(
    const G4ThreeVector &direction) const
{
  G4ThreeVector trial = RandomUnitVector();
  G4ThreeVector pol = direction.cross(trial);
  if (pol.mag2() < 1.0e-20)
  {
    trial = G4ThreeVector(1.0, 0.0, 0.0);
    pol = direction.cross(trial);
    if (pol.mag2() < 1.0e-20)
    {
      trial = G4ThreeVector(0.0, 1.0, 0.0);
      pol = direction.cross(trial);
    }
  }
  return pol.unit();
}

void StageCOpticalPrimaryGeneratorAction::GeneratePrimaries(G4Event *event)
{
  while (true)
  {
    if (!fHasCurrentSource || fNextSampleIndex >= fSamplesPerStep)
    {
      if (!ReadNextSourceRecord(fCurrentSource))
      {
        G4cout << "[StageCOpticalPrimary] No more ZnS step sources. "
               << "Aborting run at Geant4 event " << event->GetEventID()
               << G4endl;
        G4RunManager::GetRunManager()->AbortRun(true);
        return;
      }
      fHasCurrentSource = true;
      fNextSampleIndex = 0;
    }

    if (fCurrentSource.n_photon_step > 0.0)
      break;

    fNextSampleIndex = fSamplesPerStep;
  }

  const G4ThreeVector sourcePosition = SampleSourcePosition(fCurrentSource);
  const G4ThreeVector direction = RandomUnitVector();
  const G4ThreeVector polarization = RandomPolarization(direction);
  const G4double photonWeight = fCurrentSource.n_photon_step /
                                static_cast<G4double>(fSamplesPerStep);

  fCurrentPhoton.geantEventID = event->GetEventID();
  fCurrentPhoton.source = fCurrentSource;
  fCurrentPhoton.sampleIndex = fNextSampleIndex + 1;
  fCurrentPhoton.samplesPerStep = fSamplesPerStep;
  fCurrentPhoton.photonWeight = photonWeight;
  fCurrentPhoton.sourceSampling = fSourceSampling;
  fCurrentPhoton.sourcePosition = sourcePosition;

  fParticleGun->SetParticlePosition(sourcePosition);
  fParticleGun->SetParticleMomentumDirection(direction);
  fParticleGun->SetParticlePolarization(polarization);

  const G4int vertexCountBefore = event->GetNumberOfPrimaryVertex();
  fParticleGun->GeneratePrimaryVertex(event);

  if (event->GetNumberOfPrimaryVertex() > vertexCountBefore)
  {
    auto *vertex = event->GetPrimaryVertex(vertexCountBefore);
    if (vertex != nullptr && vertex->GetPrimary() != nullptr)
    {
      vertex->GetPrimary()->SetWeight(photonWeight);
    }
  }

  ++fGeneratedPhotons;
  fGeneratedPhotonWeight += photonWeight;
  ++fNextSampleIndex;
}
