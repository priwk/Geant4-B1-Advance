#ifndef StageDOpticalStats_h
#define StageDOpticalStats_h 1

#include "G4ThreeVector.hh"
#include "globals.hh"

#include <string>

struct StageDPhotonLaunchRecord
{
  G4int geantEventID = -1;
  G4double wavelength_nm = 0.0;
  std::string source_mode;
  std::string source_phase;
  G4ThreeVector source_position;
  G4ThreeVector momentum_direction;
  G4ThreeVector polarization;
  G4double photon_weight = 1.0;
  G4bool is_continuation = false;
};

struct StageDPhotonEventRecord
{
  G4int photonID = -1;
  std::string ratio;
  std::string placement_file;
  std::string source_mode;
  std::string boundary_mode;
  std::string reentry_mode;
  std::string matrix_reentry_mode;
  G4double wavelength_nm = 0.0;

  std::string source_phase;
  G4double source_x_um = 0.0;
  G4double source_y_um = 0.0;
  G4double source_z_um = 0.0;

  std::string final_status;
  G4bool absorbed = false;

  G4double total_path_length_um = 0.0;
  G4int num_steps = 0;
  G4int num_real_scatter = 0;
  G4int num_material_boundary = 0;
  G4int num_reentry = 0;
  G4int num_reentry_BN = 0;
  G4int num_reentry_ZnS = 0;
  G4int num_reentry_matrix = 0;

  G4double sum_cos_theta = 0.0;
  G4double mean_cos_theta_for_this_photon = 0.0;
  G4double weight = 1.0;
};

#endif
