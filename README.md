# Geant4-B1-Advance

BN/ZnS 局部微结构 Geant4 模型。

## Run modes

```text
/cfg/setRunMode StageA_NeutronPatch
/cfg/setRunMode StageB_ReplayAlphaLi
/cfg/setRunMode StageC_OpticalStub
```

## Source layout

```text
src/core      common configuration, geometry, physics list, action setup
src/modes     run-mode dispatchers
src/stageA    thermal-neutron patch transport and Sigma_eff summary
src/stageB    alpha/Li7 replay from neutron-capture CSV records

include/core
include/modes
include/stageA
include/stageB
```

## Stage A placement batch

Run every placement file under one or more ratio folders:

```bash
STAGEA_EVENTS=100000 bash stageA_batch_placements.sh 1-2 1-3
```

If no ratio is passed, the script scans every folder under `Input/placements`.
Each ratio writes one aggregate file:

```text
Output/stageA/<ratio>/neutron_transport_summary.csv
```

The Stage A summary table records `placement_file`, `placement_basename`, and
the placement header `seedBase`, then appends one Sigma_eff row per placement.

Stage C is currently a reserved optical-simulation interface. Optical material
properties are defined in the geometry, but `G4OpticalPhysics` and the Stage C
actions are not enabled yet.
