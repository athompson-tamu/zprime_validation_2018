// Skeletal version of CutFlow.C. Made for validation on ttbar and Z' samples.
// USAGE on Brazos:
// gSystem->Load("<path/to/your/Delphes/libDelphes");
// .x cutflow.C("/fdata/scratch/mexanick/ttbar");

#ifdef __CLING__
#include "classes/DelphesClasses.h"
#include "external/ExRootAnalysis/ExRootTreeReader.h"
#include <TROOT.h>
#include <TFile.h>
#include <TChain.h>
#include <TClonesArray.h>
#include <TLorentzVector.h>
#include <vector>
#include <string>
#include <iostream>
#include <TProfile.h>
#include <TEfficiency.h>
#endif

void cutflow(const char *sample_directory = 0) {
  gSystem->Load("libDelphes.so");

  std::string ttbar_samples;
  ttbar_samples += sample_directory;
  ttbar_samples += "/tt_*.root";

  cout << "taking samples from " << ttbar_samples.c_str() << endl;

  TChain chain_("Delphes");
  chain_.Add(ttbar_samples.c_str());

  ExRootTreeReader *treeReader = new ExRootTreeReader(&chain_);
  Long64_t numberOfEntries = treeReader->GetEntries();

  TClonesArray *branchJet = treeReader->UseBranch("Jet");
  TClonesArray *branchMuon = treeReader->UseBranch("Muon");
  TClonesArray *branchMPT = treeReader->UseBranch("MissingET");

  // Book TProfiles and TEfficiencies.
  TProfile *cutflow = new TProfile("cutflow","(2b & 2OS muons) & max(SBM)>170, MET<100, DHTLT<0;cut number;percent passing",4,-0.5,3.5);

  // Begin event loop.
  for (Int_t entry = 0; entry < numberOfEntries; ++entry) {
    if (entry % 10000 == 0) printf("On event %d / %lld \n", entry, numberOfEntries);
    treeReader->ReadEntry(entry);

    vector<int> bPosition;
    int nonbPosition = -1;
    float nonbPT = 0.;

    // If the event contains at least one jet, push back the vector of b's and
    // record the branch position of the highest-pT non-b jet.
    for (unsigned j = 0; j < branchJet->GetEntries(); ++j) {
      Jet *jet = (Jet*) branchJet->At(j);
      if (jet->PT < 30.) continue;  // Implicitly require pT > 30 GeV jets.

      if (jet->BTag) {
        bPosition.push_back(j);
      } else if (nonbPT < jet->PT) {
        nonbPosition = j;
        nonbPT = jet->PT;
      }
    }

    bool DiBottom = false;
    bool BottomJet = false;
    if (bPosition.size() >= 2) DiBottom = true;
    if (bPosition.size() == 1 && nonbPT > 0.) BottomJet = true;

    Muon *mu1, *mu2;
    bool OSMuons = false;

    // If the event contains at least 2 muons, check if they are OS.
    if (branchMuon->GetEntries() > 1) {
      mu1 = (Muon *) branchMuon->At(0);
      mu2 = (Muon *) branchMuon->At(1);

      if((mu1->Charge) != (mu2->Charge)) OSMuons = true;
    }

    // Skip to the next event if preselection requirements are unmet.
    cutflow->Fill(0, (OSMuons && (DiBottom || BottomJet)));
    if (!OSMuons) continue;
    if (!(DiBottom || BottomJet)) continue;

    // Jet assignments.
    Jet *b1, *b2;
    if(DiBottom){
      b1 = (Jet *) branchJet->At(bPosition[0]);
      b2 = (Jet *) branchJet->At(bPosition[1]);
    }
    else if(BottomJet){
      b1 = (Jet *) branchJet->At(bPosition[0]);
      b2 = (Jet *) branchJet->At(nonbPosition);
    }

    // Initialize cuts.
    bool PassMissingET = false;
    bool PassHTLT = false;
    bool PassMuJetMass = false;

    // Cut (1): Calculate max{M(mu,j)}.
    float Pm1_1 = ((mu1->P4()) + (b1->P4())).M();
    float Pm1_2 = ((mu2->P4()) + (b2->P4())).M();
    float Pm2_1 = ((mu2->P4()) + (b1->P4())).M();
    float Pm2_2 = ((mu1->P4()) + (b2->P4())).M();
    float MassPair1 = 0.;
    float MassPair2 = 0.;

    if (fabs(Pm1_1 - Pm1_2) < fabs(Pm2_1 - Pm2_2)) {
      MassPair1 = Pm1_1;
      MassPair2 = Pm1_2;
    } else {
      MassPair1 = Pm2_1;
      MassPair2 = Pm2_2;
    }

    if ((MassPair1 > 170.) || (MassPair2 > 170.)) PassMuJetMass = true;

    // Cut (2): Calculate Dimuon mass and normalized Missing ET.
    float Mmm = ((mu1->P4()) + (mu2->P4())).M();
    MissingET *MPT = (MissingET *) branchMPT->At(0);
    if ((5. * MPT->MET) < Mmm) PassMissingET = true;

    // Cut (3): Calculate HT - LT.
    float HTLT = (b1->PT) + (b2->PT) - (mu1->PT) - (mu2->PT);
    if (HTLT < 0) PassHTLT = true;

    // Fill TProfiles.
    cutflow->Fill(1, PassMuJetMass);
    if (PassMuJetMass) {
      cutflow->Fill(2, PassMissingET);
      if (PassMissingET) {
        cutflow->Fill(3, PassHTLT);
      }
    }

  } // End event loop.

  // Print out efficiency information.
  for (unsigned i=0; i<cutflow->GetNbinsX(); ++i) {
    printf("%d: %f +/- %f \n" ,i,
           cutflow->GetBinContent(i+1),
           cutflow->GetBinError(i+1));
  }
  cutflow->Draw();

}
