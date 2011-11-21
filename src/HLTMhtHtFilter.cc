/** \class HLTMhtHtFilter
*
*
*  \author Gheorghe Lungu
*
*/

#include "HLTrigger/JetMET/interface/HLTMhtHtFilter.h"
// #include "HLTrigger/JetMET/interface/AlphaT.hh"

#include "DataFormats/Common/interface/Handle.h"

#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DataFormats/JetReco/interface/CaloJetCollection.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"

#include "DataFormats/Math/interface/deltaPhi.h"

#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>
#include "TLorentzVector.h"


typedef ROOT::Math::LorentzVector<ROOT::Math::PxPyPzE4D<double> > LorentzV  ;

//
// constructors and destructor
//
HLTMhtHtFilter::HLTMhtHtFilter(const edm::ParameterSet& iConfig)
{
  inputJetTag_ = iConfig.getParameter< edm::InputTag > ("inputJetTag");
  saveTag_     = iConfig.getUntrackedParameter<bool>("saveTag",false);
  minMht_= iConfig.getParameter<double> ("minMht");
  minPtJet_= iConfig.getParameter<std::vector<double> > ("minPtJet");
  minNJet_= iConfig.getParameter<int> ("minNJet");
  mode_= iConfig.getParameter<int>("mode");
  //----mode=1 for MHT only
  //----mode=2 for Meff
  //----mode=3 for PT12
  //----mode=4 for HT only
  //----mode=5 for HT and AlphaT cross trigger (ALWAYS uses jet ET, not pT)
  etaJet_= iConfig.getParameter<std::vector<double> > ("etaJet");
  usePt_= iConfig.getParameter<bool>("usePt");
  minPT12_= iConfig.getParameter<double> ("minPT12");
  minMeff_= iConfig.getParameter<double> ("minMeff");
  minHt_= iConfig.getParameter<double> ("minHt");
  minAlphaT_= iConfig.getParameter<double> ("minAlphaT");

  // sanity checks
  if (       (minPtJet_.size()    !=  etaJet_.size())
       || (  (minPtJet_.size()<1) || (etaJet_.size()<1) )
       || ( ((minPtJet_.size()<2) || (etaJet_.size()<2))
      && ( (mode_==1) || (mode_==2) || (mode_ == 5))) ) {
    edm::LogError("HLTMhtHtFilter") << "inconsistent module configuration!";
  }

  //register your products
  produces<trigger::TriggerFilterObjectWithRefs>();
}

HLTMhtHtFilter::~HLTMhtHtFilter(){}

void HLTMhtHtFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("inputJetTag",edm::InputTag("hltMCJetCorJetIcone5HF07"));
  desc.addUntracked<bool>("saveTag",false);
  desc.add<double>("minMht",0.0);
  {
    std::vector<double> temp1;
    temp1.reserve(2);
    temp1.push_back(20.0);
    temp1.push_back(20.0);
    desc.add<std::vector<double> >("minPtJet",temp1);
  }
  desc.add<int>("minNJet",0);
  desc.add<int>("mode",2);
  {
    std::vector<double> temp1;
    temp1.reserve(2);
    temp1.push_back(9999.0);
    temp1.push_back(9999.0);
    desc.add<std::vector<double> >("etaJet",temp1);
  }
  desc.add<bool>("usePt",true);
  desc.add<double>("minPT12",0.0);
  desc.add<double>("minMeff",180.0);
  desc.add<double>("minHt",0.0);
  desc.add<double>("minAlphaT",0.0);
  descriptions.add("hltMhtHtFilter",desc);
}



// ------------ method called to produce the data  ------------
bool
  HLTMhtHtFilter::filter(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace std;
  using namespace edm;
  using namespace reco;
  using namespace trigger;
  // The filter object
  auto_ptr<trigger::TriggerFilterObjectWithRefs> filterobject (new trigger::TriggerFilterObjectWithRefs(path(),module()));
  if (saveTag_) filterobject->addCollectionTag(inputJetTag_);

  CaloJetRef ref;
  // Get the Candidates

  Handle<CaloJetCollection> recocalojets;
  iEvent.getByLabel(inputJetTag_,recocalojets);

  // look at all candidates,  check cuts and add to filter object
  int n(0), nj(0), flag(0);
  double ht=0.;
  double mhtx=0., mhty=0.;
  double jetVar;
  double dht = 0.;
  double aT = 0.;
  if(recocalojets->size() > 0){
    // events with at least one jet
    //make a collection of jets to push back in to alphaT
    std::vector<LorentzV> jets;
    for (CaloJetCollection::const_iterator recocalojet = recocalojets->begin();
    recocalojet != recocalojets->end(); recocalojet++) {
      if (flag == 1){break;}
      jetVar = recocalojet->pt();
      if (!usePt_ || mode_==3 ) jetVar = recocalojet->et();

      if (mode_==1 || mode_==2 || mode_ == 5) {//---get MHT
        if (jetVar > minPtJet_.at(1) && fabs(recocalojet->eta()) < etaJet_.at(1)) {
          mhtx -= jetVar*cos(recocalojet->phi());
          mhty -= jetVar*sin(recocalojet->phi());
        }
      }
      if (mode_==2 || mode_==4 || mode_==5) {//---get HT
        if (jetVar > minPtJet_.at(0) && fabs(recocalojet->eta()) < etaJet_.at(0)) {
          ht += jetVar;
          nj++;
          LorentzV JetLVec(0.,0.,0.,0.);
          JetLVec.SetPtEtaPhiM(recocalojet->pt(),recocalojet->eta(),recocalojet->phi(),recocalojet->mass());
          jets.push_back( JetLVec );
        }
      }
      if (mode_==3) {//---get PT12
        if (jetVar > minPtJet_.at(0) && fabs(recocalojet->eta()) < etaJet_.at(0)) {
          nj++;
          mhtx -= jetVar*cos(recocalojet->phi());
          mhty -= jetVar*sin(recocalojet->phi());
          if (nj==2) break;
        }
      }
      if(mode_ == 5){
        double mHT = sqrt( (mhtx*mhtx) + (mhty*mhty) );
        dht += ( nj < 2 ? jetVar : -1.* jetVar ); //@@ only use for njets < 4

        std::vector<double> et;
        std::vector<double> px;
        std::vector<double> py;

        double alphaT = 0.;
        transform( jets.begin(), jets.end(), back_inserter(et), std::mem_fun_ref(&LorentzV::Et) );
        transform( jets.begin(), jets.end(), back_inserter(px), std::mem_fun_ref(&LorentzV::Px) );
        transform( jets.begin(), jets.end(), back_inserter(py), std::mem_fun_ref(&LorentzV::Py) );



        const double sum_et = accumulate( et.begin(), et.end(), 0. );
        const double sum_px = accumulate( px.begin(), px.end(), 0. );
        const double sum_py = accumulate( py.begin(), py.end(), 0. );

    double min_delta_sum_et = -1.;
    for ( unsigned i=0; i < unsigned(1<<(et.size()-1)); i++ ) { //@@ iterate through different combinations
      double delta_sum_et = 0.;
      std::vector<bool> jet;
      for ( unsigned j=0; j < et.size(); j++ ) { //@@ iterate through jets
        delta_sum_et += et[j] * ( 1 - 2 * (int(i>>j)&1) );

      }
      if ( ( fabs(delta_sum_et) < min_delta_sum_et || min_delta_sum_et < 0. ) ) {
        min_delta_sum_et = fabs(delta_sum_et);
      }
    }
    if ( min_delta_sum_et < 0. ) { alphaT =  0.; }

    // Alpha_T
     alphaT =  ( 0.5 * ( sum_et - min_delta_sum_et ) / sqrt( sum_et*sum_et - (sum_px*sum_px+sum_py*sum_py) ) );


        if ( nj == 2 || nj == 3 ) {
          aT = ( ht - fabs(dht) ) / ( 2. * sqrt( ( ht*ht ) - ( mHT*mHT  ) ) );
        } else if ( nj > 3 ) {
          aT = ht / ( 2.*sqrt( ( ht*ht ) - ( mHT*mHT  ) ) );
        }
        std::cout << "AlphaT from full calculation = " << alphaT << " ALphaT from approximation = " << aT << std::endl;
        if(ht > minHt_ && aT > minAlphaT_){
  // put filter object into the Event
          flag = 1;
        }
      }
    }

  if( mode_==1 && sqrt(mhtx*mhtx + mhty*mhty) > minMht_) flag=1;
  if( mode_==2 && sqrt(mhtx*mhtx + mhty*mhty)+ht > minMeff_) flag=1;
  if( mode_==3 && sqrt(mhtx*mhtx + mhty*mhty) > minPT12_ && nj>1) flag=1;
  if( mode_==4 && ht > minHt_) flag=1;

  if (flag==1) {
    for (reco::CaloJetCollection::const_iterator recocalojet = recocalojets->begin(); recocalojet!=recocalojets->end(); recocalojet++) {
      jetVar = recocalojet->pt();
      if (!usePt_ || mode_==3) jetVar = recocalojet->et();

      if (jetVar > minPtJet_.at(0)) {
        ref = CaloJetRef(recocalojets,distance(recocalojets->begin(),recocalojet));
        filterobject->addObject(TriggerJet,ref);
        n++;
      }
    }
  }
} // events with at least one jet



  // filter decision
bool accept(n>0);

  // put filter object into the Event
iEvent.put(filterobject);

return accept;
}
