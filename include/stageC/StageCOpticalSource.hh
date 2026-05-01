#ifndef StageCOpticalSource_h
#define StageCOpticalSource_h 1

#include "G4ThreeVector.hh"
#include "globals.hh"

#include <string>

struct StageCSourceRecord
{
  std::string source_event_uid;
  std::string source_step_uid;

  G4int eventID = -1;
  G4int trackID = -1;
  G4int stepID = -1;
  std::string particle;

  G4double thickness_um = 0.0;
  G4double bn_wt = 0.0;
  G4double zns_wt = 0.0;
  std::string ratio_label;
  G4double depth_um = 0.0;
  G4double capture_depth_um = 0.0;
  std::string placement_file;
  std::string placement_hash;

  G4ThreeVector xPre;
  G4ThreeVector xPost;
  G4double step_len_um = 0.0;

  G4double edep_keV = 0.0;
  G4double visible_edep_keV = 0.0;
  G4double n_photon_step = 0.0;
};

struct StageCPhotonRecord
{
  G4int geantEventID = -1;
  StageCSourceRecord source;

  G4int sampleIndex = 0;
  G4int samplesPerStep = 1;
  G4double photonWeight = 0.0;
  std::string sourceSampling;

  G4ThreeVector sourcePosition;
};

#endif
