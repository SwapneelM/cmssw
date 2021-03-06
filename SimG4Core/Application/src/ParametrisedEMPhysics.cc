//
// Joanna Weng 08.2005
// Physics process for Gflash parameterisation
// modified by Soon Yung Jun, Dongwook Jang
// V.Ivanchenko rename the class, cleanup, and move
//              to SimG4Core/Application - 2012/08/14

#include "SimG4Core/Application/interface/ParametrisedEMPhysics.h"
#include "SimG4Core/Application/interface/GFlashEMShowerModel.h"
#include "SimG4Core/Application/interface/GFlashHadronShowerModel.h"
#include "SimG4Core/Application/interface/ElectronLimiter.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "G4FastSimulationManagerProcess.hh"
#include "G4ProcessManager.hh"

#include "G4LeptonConstructor.hh"
#include "G4MesonConstructor.hh"
#include "G4BaryonConstructor.hh"
#include "G4ShortLivedConstructor.hh"
#include "G4IonConstructor.hh"
#include "G4RegionStore.hh"
#include "G4Electron.hh"
#include "G4Positron.hh"
#include "G4MuonMinus.hh"
#include "G4MuonPlus.hh"
#include "G4PionMinus.hh"
#include "G4PionPlus.hh"
#include "G4KaonMinus.hh"
#include "G4KaonPlus.hh"
#include "G4Proton.hh"
#include "G4AntiProton.hh"

#include "G4EmParameters.hh"
#include "G4PhysicsListHelper.hh"
#include "G4SystemOfUnits.hh"
#include "G4UAtomicDeexcitation.hh"
#include "G4LossTableManager.hh"
#include "G4ProcessManager.hh"
#include "G4Transportation.hh"

G4ThreadLocal std::unique_ptr<GFlashEMShowerModel> ParametrisedEMPhysics::theEcalEMShowerModel;
G4ThreadLocal std::unique_ptr<GFlashEMShowerModel> ParametrisedEMPhysics::theHcalEMShowerModel;
G4ThreadLocal std::unique_ptr<GFlashHadronShowerModel> ParametrisedEMPhysics::theEcalHadShowerModel;
G4ThreadLocal std::unique_ptr<GFlashHadronShowerModel> ParametrisedEMPhysics::theHcalHadShowerModel;
G4ThreadLocal std::unique_ptr<ElectronLimiter> ParametrisedEMPhysics::theElectronLimiter;
G4ThreadLocal std::unique_ptr<ElectronLimiter> ParametrisedEMPhysics::thePositronLimiter;
G4ThreadLocal std::unique_ptr<G4FastSimulationManagerProcess> ParametrisedEMPhysics::theFastSimulationManagerProcess; 

ParametrisedEMPhysics::ParametrisedEMPhysics(std::string name, 
					     const edm::ParameterSet & p) 
  : G4VPhysicsConstructor(name), theParSet(p) 
{
  theEcalEMShowerModel.reset(nullptr);
  theEcalHadShowerModel.reset(nullptr);
  theHcalEMShowerModel.reset(nullptr);
  theHcalHadShowerModel.reset(nullptr);
  theFastSimulationManagerProcess.reset(nullptr); 

  // bremsstrahlung threshold and EM verbosity
  G4EmParameters* param = G4EmParameters::Instance();
  G4int verb = theParSet.getUntrackedParameter<int>("Verbosity",0);
  param->SetVerbose(verb);

  G4double bremth = theParSet.getParameter<double>("G4BremsstrahlungThreshold")*GeV; 
  param->SetBremsstrahlungTh(bremth);

  bool fluo = theParSet.getParameter<bool>("FlagFluo");
  param->SetFluo(fluo);

  bool modifyT = theParSet.getParameter<bool>("ModifyTransportation");
  double th1 = theParSet.getUntrackedParameter<double>("ThresholdWarningEnergy")*MeV;
  double th2 = theParSet.getUntrackedParameter<double>("ThresholdImportantEnergy")*MeV;
  int nt = theParSet.getUntrackedParameter<int>("ThresholdTrials");

  edm::LogVerbatim("SimG4CoreApplication") 
    << "ParametrisedEMPhysics::ConstructProcess: bremsstrahlung threshold Eth= "
    << bremth/GeV << " GeV" 
    << "\n                                         verbosity= " << verb
    << "  fluoFlag: " << fluo << "  modifyTransport: " << modifyT 
    << "  Ntrials= " << nt 
    << "\n                                         ThWarning(MeV)= " << th1
    << "  ThException(MeV)= " << th2;

  // Russian roulette and tracking cut for e+-
  double energyLim = 
    theParSet.getParameter<double>("RusRoElectronEnergyLimit")*MeV;
  if(energyLim > 0.0) {
    const G4int NREG = 6; 
    const G4String rname[NREG] = {"EcalRegion", "HcalRegion", "MuonIron",
                                  "PreshowerRegion","CastorRegion",
                                  "DefaultRegionForTheWorld"};
    G4double rrfact[NREG] = { 1.0 };

    rrfact[0] = theParSet.getParameter<double>("RusRoEcalElectron");
    rrfact[1] = theParSet.getParameter<double>("RusRoHcalElectron");
    rrfact[2] = theParSet.getParameter<double>("RusRoMuonIronElectron");
    rrfact[3] = theParSet.getParameter<double>("RusRoPreShowerElectron");
    rrfact[4] = theParSet.getParameter<double>("RusRoCastorElectron");
    rrfact[5] = theParSet.getParameter<double>("RusRoWorldElectron");
    for(int i=0; i<NREG; ++i) {
      if(rrfact[i] < 1.0) {
	param->ActivateSecondaryBiasing("eIoni",rname[i],rrfact[i],energyLim);
	param->ActivateSecondaryBiasing("hIoni",rname[i],rrfact[i],energyLim);
	edm::LogVerbatim("SimG4CoreApplication") 
	  << "ParametrisedEMPhysics: Russian Roulette"
	  << " for e- Prob= " << rrfact[i]  
	  << " Elimit(MeV)= " << energyLim/CLHEP::MeV
	  << " inside " << rname[i];
      }
    }
  }
}

ParametrisedEMPhysics::~ParametrisedEMPhysics() {
}

void ParametrisedEMPhysics::ConstructParticle() 
{
  G4LeptonConstructor pLeptonConstructor;
  pLeptonConstructor.ConstructParticle();

  G4MesonConstructor pMesonConstructor;
  pMesonConstructor.ConstructParticle();

  G4BaryonConstructor pBaryonConstructor;
  pBaryonConstructor.ConstructParticle();

  G4ShortLivedConstructor pShortLivedConstructor;
  pShortLivedConstructor.ConstructParticle();  
    
  G4IonConstructor pConstructor;
  pConstructor.ConstructParticle();  
}

void ParametrisedEMPhysics::ConstructProcess() {

  // GFlash part 
  bool gem  = theParSet.getParameter<bool>("GflashEcal");
  bool ghad = theParSet.getParameter<bool>("GflashHcal");
  bool gemHad  = theParSet.getParameter<bool>("GflashEcalHad");
  bool ghadHad = theParSet.getParameter<bool>("GflashHcalHad");

  G4PhysicsListHelper* ph = G4PhysicsListHelper::GetPhysicsListHelper();
  if(gem || ghad || gemHad || ghadHad) {
    edm::LogInfo("SimG4CoreApplication") 
      << "ParametrisedEMPhysics: GFlash Construct for e+-: " 
      << gem << "  " << ghad << " for hadrons: " << gemHad << "  " << ghadHad;

    theFastSimulationManagerProcess.reset(new G4FastSimulationManagerProcess());

    if(gem || ghad) {
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4Electron::Electron());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4Positron::Positron());
    }
    if(gemHad || ghadHad) {
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4Proton::Proton());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4AntiProton::AntiProton());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4PionPlus::PionPlus());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4PionMinus::PionMinus());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4KaonPlus::KaonPlus());
      ph->RegisterProcess(theFastSimulationManagerProcess.get(), G4KaonMinus::KaonMinus());
    }

    if(gem || gemHad) {
      G4Region* aRegion = 
	G4RegionStore::GetInstance()->GetRegion("EcalRegion");
      
      if(!aRegion){
	edm::LogVerbatim("SimG4CoreApplication") 
	  << "ParametrisedEMPhysics::ConstructProcess: " 
	  << "EcalRegion is not defined, GFlash will not be enabled for ECAL!";
	
      } else {
	if(gem) {

	  //Electromagnetic Shower Model for ECAL
	  theEcalEMShowerModel.reset(new GFlashEMShowerModel("GflashEcalEMShowerModel",
                                                             aRegion,theParSet));
	}
	if(gemHad) {

	  //Electromagnetic Shower Model for ECAL
	  theEcalHadShowerModel.reset(new GFlashHadronShowerModel("GflashEcalHadShowerModel",
                                                                  aRegion,theParSet));
	}    
      }
    }
    if(ghad || ghadHad) {
      G4Region* aRegion = 
	G4RegionStore::GetInstance()->GetRegion("HcalRegion");
      if(!aRegion) {
	edm::LogVerbatim("SimG4CoreApplication") 
	  << "ParametrisedEMPhysics::ConstructProcess: " 
	  << "HcalRegion is not defined, GFlash will not be enabled for HCAL!";
	
      } else {
	if(ghad) {

	  //Electromagnetic Shower Model for HCAL
	  theHcalEMShowerModel.reset(new GFlashEMShowerModel("GflashHcalEMShowerModel",
                                                             aRegion,theParSet));
	}
	if(ghadHad) {

	  //Electromagnetic Shower Model for ECAL
	  theHcalHadShowerModel.reset(new GFlashHadronShowerModel("GflashHcalHadShowerModel",
                                                                  aRegion,theParSet));
	}    
      }
    }
  }

  // Step limiters for e+-
  bool eLimiter = theParSet.getParameter<bool>("ElectronStepLimit");
  bool rLimiter = theParSet.getParameter<bool>("ElectronRangeTest");
  bool pLimiter = theParSet.getParameter<bool>("PositronStepLimit");

  if(eLimiter || rLimiter) {
    theElectronLimiter.reset(new ElectronLimiter(theParSet));
    theElectronLimiter.get()->SetRangeCheckFlag(rLimiter);
    theElectronLimiter.get()->SetFieldCheckFlag(eLimiter);
    ph->RegisterProcess(theElectronLimiter.get(), G4Electron::Electron());
  }
  
  if(pLimiter){
    thePositronLimiter.reset(new ElectronLimiter(theParSet));
    thePositronLimiter.get()->SetFieldCheckFlag(pLimiter);
    ph->RegisterProcess(theElectronLimiter.get(), G4Positron::Positron());
  }
  // enable fluorescence
  bool fluo = theParSet.getParameter<bool>("FlagFluo");
  if(fluo && !G4LossTableManager::Instance()->AtomDeexcitation()) {
    G4VAtomDeexcitation* de = new G4UAtomicDeexcitation();
    G4LossTableManager::Instance()->SetAtomDeexcitation(de);
  }
  // chnage parameters of transportation
  bool modifyT = theParSet.getParameter<bool>("ModifyTransportation");
  if(modifyT) {
    G4ProcessManager* man = G4Electron::Electron()->GetProcessManager();
    G4Transportation* trans = (G4Transportation*)((*(man->GetProcessList()))[0]);
    if(trans) {
      double th1 = theParSet.getUntrackedParameter<double>("ThresholdWarningEnergy")*MeV;
      double th2 = theParSet.getUntrackedParameter<double>("ThresholdImportantEnergy")*MeV;
      int nt = theParSet.getUntrackedParameter<int>("ThresholdTrials");
      trans->SetThresholdWarningEnergy(th1); 
      trans->SetThresholdImportantEnergy(th2); 
      trans->SetThresholdTrials(nt); 
    }
  }
}
