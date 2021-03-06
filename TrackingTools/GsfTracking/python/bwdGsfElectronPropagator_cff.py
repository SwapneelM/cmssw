import FWCore.ParameterSet.Config as cms

import TrackingTools.MaterialEffects.OppositeMaterialPropagator_cfi
# "backward" propagator for electrons
bwdGsfElectronPropagator = TrackingTools.MaterialEffects.OppositeMaterialPropagator_cfi.OppositeMaterialPropagator.clone()
bwdGsfElectronPropagator.Mass = 0.000511
bwdGsfElectronPropagator.ComponentName = 'bwdGsfElectronPropagator'

