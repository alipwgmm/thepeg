// -*- C++ -*-
//
// This is the implementation of the non-inlined, non-templated member
// functions of the EventFiller class.
//

#include "DipoleEventHandler.h"
#include "EventFiller.h"
#include "ThePEG/Interface/Switch.h"
#include "ThePEG/Interface/Parameter.h"
#include "ThePEG/Repository/EventGenerator.h"
#include "ThePEG/Repository/CurrentGenerator.h"
#include "ThePEG/Utilities/Current.h"
#include "ThePEG/Utilities/Throw.h"
#include "ThePEG/Repository/UseRandom.h"
#include "ThePEG/Repository/EventGenerator.h"
#include "ThePEG/Repository/CurrentGenerator.h"
#include "ThePEG/EventRecord/Step.h"
#include "ThePEG/EventRecord/Particle.h"
#include "ThePEG/EventRecord/SubProcess.h"
#include "RealPartonState.h"
#include "ParticleInfo.h"
#include "ThePEG/Utilities/Debug.h"
#include "ThePEG/Utilities/DebugItem.h"
#include "ThePEG/Utilities/UtilityBase.h"
#include "ThePEG/Utilities/SimplePhaseSpace.h"
#include "ThePEG/Handlers/LuminosityFunction.h"

#include "ThePEG/Utilities/DescribeClass.h"
#include "ThePEG/Interface/ClassDocumentation.h"
#include "ThePEG/Interface/Reference.h"

#include "ThePEG/Persistency/PersistentOStream.h"
#include "ThePEG/Persistency/PersistentIStream.h"

#include "../Cascade/EmitterBase.h"
#include "Sum20Momentum.h"

#include <iostream>
#include <fstream>

using namespace DIPSY;

EventFiller::EventFiller()
  : currentWeight(0.0), theRecoilScheme(0), theSingleMother(0),
    theDGLAPinPT(0), theEffectiveWeights(0), theFSSwingTime(0.0),
    theFSSwingTimeStep(0.1),
    theValenceChargeNormalisation(0), thePTCut(ZERO), theSoftRemove(1),
    onlyOnce(false), compat(0), debughistos(0) {}
EventFiller::~EventFiller() {}

IBPtr EventFiller::clone() const {
  return new_ptr(*this);
}

IBPtr EventFiller::fullclone() const {
  return new_ptr(*this);
}

double EventFiller::fill(Step & step, DipoleEventHandler & eh, tPPair inc,
			 DipoleState & dl, DipoleState & dr,
			 const ImpactParameters & b) const {

  
  if ( dl.hasShadows() ) return fillWithShadows(step, eh, inc, b, dl, dr);
  if ( mode() == 4 ) return fill(step, eh, inc, b, dl, dr);

  // Get a list of possible dipole-dipole interactions
  FList fl = eh.xSecFn().flist(dl, dr, b);

  //Sum over the (ununitarised) interaction probabilities 2*f_{ij}.
  //Note that the factor 2 is included in flist(), to give the correct
  //ND interaction probability.
  double sum = 0.0;
  for ( FList::iterator it = fl.begin(); it != fl.end(); ++it )
    sum += it->first.first;

  //Just check that they are not all 0.
  if ( sum == 0.0 ) {
    return 0.0;
  }

  //Non-diffractive interaction probability is 1 - exp(-Sum(2*f_{ij}))
  double weight = eh.xSecFn().unitarize(sum);

  //Combine the weights from all the sources.
  currentWeight =
    weight*sqr(hbarc)*dr.weight()*dl.weight()*b.weight()/eh.maxXSec();

  if ( histdd0)
    histdd0->fill(dl.getDipoles().size()*dr.getDipoles().size() + 0.5,
		  currentWeight);

  // Select the interactions which should be performed.
  pair<RealPartonStatePtr, RealPartonStatePtr> realStates = 
    selectInteractions(fl, b, eh.xSecFn());

  //If no interactions found, discard event.
  if ( realStates.first->interactions.empty() ) {
    return 0.0;
  }

  //Counts the number of participants in a HI event.
  //TODO: interface, or trigger on AA.
  countParticipants(dl, dr, b.bVec().pt());

  //Figure out which partons to keep, and which to remove.
  //Also sort out colour flow, momenta etc.
  vector<String> strings = extractStrings(dl, dr, realStates, b);
  DipoleState & finalState = dl;

  //Discard event if no final state partons were found.
  if ( strings.empty() || ! fillStep(step, inc, strings) ) {
    weight = 0.0;
    currentWeight = 0.0;
    return weight;
  }

  // The last thing we do is to fix up valens configurations.
  finalState.fixValence(step);

  return weight;
}

double EventFiller::
fill(Step & step, DipoleEventHandler & eh, tPPair inc,
     const ImpactParameters & b, DipoleState & dl, DipoleState & dr) const {

  if ( dl.hasShadows() ) return fillWithShadows(step, eh, inc, b, dl, dr);

  // Get a list of possible dipole-dipole interactions
  InteractionList intl = eh.xSecFn().flist(b, dl, dr);

  //Sum over the (ununitarised) interaction probabilities 2*f_{ij}.
  //ND interaction probability.
  double sum = 0.0;
  for ( InteractionList::iterator it = intl.begin(); it != intl.end(); ++it )
    sum += it->f2;

  //Just check that they are not all 0.
  if ( sum <= 0.0 ) {
    return 0.0;
  }

  //Non-diffractive interaction probability is 1 - exp(-Sum(2*f_{ij}))
  double weight = eh.xSecFn().unitarize(sum);

  //Combine the weights from all the sources.
  currentWeight =
    weight*sqr(hbarc)*dr.weight()*dl.weight()*b.weight()/eh.maxXSec();

  if ( histdd0 )
    histdd0->fill(dl.getDipoles().size()*dr.getDipoles().size() + 0.5,
		  currentWeight);

  // Select the interactions which should be performed.
  pair<RealPartonStatePtr, RealPartonStatePtr> realStates = 
    selectInteractions(intl, eh.xSecFn());

  //If no interactions found, discard event.
  if ( realStates.first->interactions.empty() ) {
    return 0.0;
  }

  //Counts the number of participants in a HI event.
  //TODO: interface, or trigger on AA.
  countParticipants(dl, dr, b.bVec().pt());

  //Figure out which partons to keep, and which to remove.
  //Also sort out colour flow, momenta etc.
  vector<String> strings = extractStrings(dl, dr, realStates, b);
  DipoleState & finalState = dl;

  //Discard event if no final state partons were found.
  if ( strings.empty() || ! fillStep(step, inc, strings) ) {
    weight = 0.0;
    currentWeight = 0.0;
    return weight;
  }

  // The last thing we do is to fix up valens configurations.
  finalState.fixValence(step);

  return weight;
}

double EventFiller::
fillWithShadows(Step & step, DipoleEventHandler & eh, tPPair inc,
     const ImpactParameters & b, DipoleState & dl, DipoleState & dr) const {

  // Get a list of possible dipole-dipole interactions
  InteractionList intl = eh.xSecFn().flist(b, dl, dr);

  //Sum over the (ununitarised) interaction probabilities 2*f_{ij}.
  //ND interaction probability.
  double sum = 0.0;
  for ( InteractionList::iterator it = intl.begin(); it != intl.end(); ++it )
    if ( it->status == DipoleInteraction::ACCEPTED ) sum += it->f2;

  //Just check that they are not all 0.
  if ( intl.empty() || sum <= 0.0 ) return 0.0;

  //Non-diffractive interaction probability is 1 - exp(-Sum(2*f_{ij}))
  double weight = eh.xSecFn().unitarize(sum);

  //Combine the weights from all the sources.
  currentWeight =
    weight*sqr(hbarc)*dr.weight()*dl.weight()*b.weight()/eh.maxXSec();

  InteractionList cleaned;
  for ( InteractionList::iterator it = intl.begin(); it != intl.end(); ++it ) {
    if ( histf0 ) {
      histf0->fill(it->uf2, currentWeight);
      switch ( it->status ) {
      case DipoleInteraction::ACCEPTED:
	histfa->fill(it->uf2, currentWeight);
	break;
      case DipoleInteraction::PROPFAIL:
	histfp->fill(it->uf2, currentWeight);
	break;
      case DipoleInteraction::KINEFAIL:
	histfk->fill(it->uf2, currentWeight);
	break;
      case DipoleInteraction::ORDERING:
	histfo->fill(it->uf2, currentWeight);
	break;
      case DipoleInteraction::UNKNOWN:
	histff->fill(it->uf2, currentWeight);
	break;
      }
    }
    if ( it->status == DipoleInteraction::ACCEPTED ) cleaned.insert(*it);
  }
  intl.swap(cleaned);

  if ( histdd0 ) {
    double nij = dl.getDipoles().size()*dr.getDipoles().size() + 0.5;
    histdd0->fill(nij, currentWeight);
    histddra->fill(nij, currentWeight*eh.xSecFn().nIAccepted/nij);
    histddrf->fill(nij, currentWeight*eh.xSecFn().nIBelowCut/nij);
    histddrp->fill(nij, currentWeight*eh.xSecFn().nIPropFail/nij);
    histddrk->fill(nij, currentWeight*eh.xSecFn().nIKineFail/nij);
    histddro->fill(nij, currentWeight*eh.xSecFn().nIOrdering/nij);
  }

  InteractionVector interactions =  selectShadowInteractions(intl, eh.xSecFn());

  //If no interactions found, discard event.
  if ( interactions.empty() ) return 0.0;


  //Figure out which partons to keep, and which to remove.
  //Also sort out colour flow, momenta etc.
  vector<String> strings = extractShadowStrings(dl, dr, interactions, b);
  DipoleState & finalState = dl;

  //Discard event if no final state partons were found.
  if ( strings.empty() || ! fillStep(step, inc, strings) ) {
    weight = 0.0;
    currentWeight = 0.0;
    return weight;
  }
  finalState.checkFSMomentum(step);

  // The last thing we do is to fix up valens configurations.
  finalState.fixValence(step);
  finalState.checkFSMomentum(step);

  return weight;
}

void EventFiller::countParticipants(const DipoleState & dl,
				    const DipoleState & dr, const InvEnergy b) const {
  //The initial dipoles. Note that these will in general have emitted
  //something, and thus no longer be active. They may have been
  //reconnected by a new dipole in the absorption process though.
  vector<DipolePtr> valenceDip = dl.initialDipoles();
  valenceDip.insert(valenceDip.end(),
		    dr.initialDipoles().begin(), dr.initialDipoles().end());

  //count the nonparticipating nucleons
  int untouchedNucleons = 0;
  for ( int i = 0; i < int(valenceDip.size()); i++ ) {
    //First find the three valence partons in the nucleon that
    //the dipole belongs to.
    //Two are connected to the dipole, easy.
    tPartonPtr p1 = valenceDip[i]->partons().first;
    tPartonPtr p2 = valenceDip[i]->partons().second;
    //The three dipoles in valenceDip are always after each other,
    //so the third parton can be found in the dipole before or
    //after (or both).
    tPartonPtr p3;
    if ( i != int(valenceDip.size())-1 &&
	 valenceDip[i+1]->partons().first == p2  )
      p3 = valenceDip[i+1]->partons().second;
    else if ( i != int(valenceDip.size())-1 &&
	      valenceDip[i+1]->partons().second == p1  )
      p3 = valenceDip[i+1]->partons().first;
    else if ( i != 0 && valenceDip[i-1]->partons().first == p2  )
      p3 = valenceDip[i-1]->partons().second;
    else if ( i != 0 && valenceDip[i-1]->partons().second == p1  )
      p3 = valenceDip[i-1]->partons().first;

    //If none of the valence partons have interacted
    //ie (have children that interacted), the nucleon is considered
    //to not have interacted. Note that with swings, a single interaction
    //can make several nucleons participating.
    if ( !(p1->interacted()) &&
	 !(p2->interacted()) &&
	 !( p3 && p3->interacted() )  ) {
      valenceDip[i]->participating(false);
      untouchedNucleons++;
    }
    else
      valenceDip[i]->participating(true);
  }

  //We will triple count, as we loop through all 3 dipoles in
  //each nucleon. Divide by 3 to make up for this.
  untouchedNucleons /= 3;
}

/*
 * This is the probability that a parton is interacting, given that
 * the state interacts, but the partons is not selected as primary.
 *
 * Prob of p interacting: p
 * Prob of p interacting, given that the state interacts: x = p/totalP
 * Prob of p being selected as primary, given state interacts:  y = p/sumP
 * Prob of p being selected as non-primary, given state
 * interacts: z
 * x = y + z ==> z = p/totalP - p/sumP 
 * Prob of p not being primary, given state interacts: A = (sumP-p)/sumP
 * And now what we are looking for:
 * Prob of p being selected as non-primary, given state interacts
 * and not selected as primary: B
 * z = A*B ==> B = z/A = p(1/totalP - 1/sumP)/((sumP-p)/sumP) =
 * = p(sumP/totalP - 1)/(sumP-p)
 */
double correctedProb(double totalP, double sumP, double p) {
  return p*(sumP/totalP - 1.0)/(sumP - p);
}

pair<RealPartonStatePtr, RealPartonStatePtr>
EventFiller::selectInteractions(const FList & fl, const ImpactParameters & b,
				const DipoleXSec & xSec) const {

  if ( histddi ) histddi->fill(fl.size() + 0.5, currentWeight);

  double sumfij = 0.0; //the sum of the individual nonuntarised int probs
  double sumUP = 0.0;   //The sum of the individual unitarised int probs

  //Set up a selector, and a map sorted on pt of the interaction.
  Selector<FList::const_iterator, double> sel;
  DipolePairMap ordered;

  //Loop through all possible interactions and fill
  //@sel and @ordered.
  for ( FList::const_iterator it = fl.begin(); it != fl.end(); ++it ) {

    //If no interaction probability, don't bother.
    if ( it->first.second == 0.0 ) continue;

    //insert into the selector.
    sel.insert(it->first.second, it);

    sumfij += it->first.first;
    sumUP += it->first.second;

    if ( histfa ) histfa->fill(it->first.second, currentWeight);

    //Calculate interaction recoils by calling the DipoleXSec object.
    DipoleXSec::InteractionRecoil rec =
      xSec.recoil(it->second.first->partons(), it->second.second->partons(), b);

    //Insert in @ordered, sorting on max pt recoil.
    double pt = max(max(rec.first.first.pt(), rec.first.second.pt()),
		    max(rec.second.first.pt(), rec.second.second.pt()))/GeV;
    ordered.insert(make_pair(pt, it));
    //    ordered.insert(make_pair(it->first.first, it));
  }

  //interaction probability (for at least one interaction)
  //of the two states.
  double totalP = Current<DipoleEventHandler>()->xSecFn().unitarize(sumfij);

  //create the real states.
  RealPartonStatePtr rrs = new_ptr(RealPartonState());
  RealPartonStatePtr lrs = new_ptr(RealPartonState());

  //Add the valence partons (as they are always real) and save.
  lrs->addValence(fl.begin()->second.first->dipoleState());
  rrs->addValence(fl.begin()->second.second->dipoleState());
  lrs->saveState();
  rrs->saveState();

  DipolePairVector interactions;
  DipolePairMap potential;
  DipolePairMap failedPrims;
  bool found = false;
  int counter = 0;
  double maxpt = 0.0;
  double maxpte = 0.0;
  while ( !found ) {
    potential.clear();
    interactions.clear();
    maxpt = 0.0;
    maxpte = 0.0;

    //select a first interaction (since there has to be at least one).
    FList::const_iterator prim = sel[UseRandom::rnd()];

    //Go through the other interactions and check if they are also
    //interacting. A modified probability is used, to make up for the
    //bias introduced in selecting a primary interaction.
    for ( FList::const_iterator it = fl.begin(); it != fl.end(); ++it )
      if ( it == prim || correctedProb(totalP,sumUP,it->first.second) 
	   > UseRandom::rnd() ) {
	// DipoleXSec::InteractionRecoil rec =
	//   xSec.recoil(it->second.first->partons(), it->second.second->partons(), 
	//		      b);
	// double pt = max(max(rec.first.first.pt(), rec.first.second.pt()),
	// 		max(rec.second.first.pt(), rec.second.second.pt()))/GeV;

	//If the interaction passed the amplitude test, add it to the list
	//of potential interations.
	//	potential.insert(make_pair(pt, it));
	if ( histff ) histff->fill(it->first.second, currentWeight);
	potential.insert(make_pair(it->first.first, it));
      }

    //Keep track on how many times we have tried to find a consistent
    //set of interactions. Give up after 10 tries.
    counter++;
    if ( counter > 10 ) {
      overTenEvents += currentWeight;
      return make_pair(lrs, rrs);
    }

    //test all potetential interactions in order, skipping already failed dips.
    //failed first interactions inserted in failedprims,
    //and from sel and ordered if first in ordered.
    int i = 0;
    set<tDipolePtr> ldips, rdips;
    for ( DipolePairMap::iterator it = potential.begin(); it != potential.end(); ++it ) {
      i++;

      //Check first interaction, and already failed as first.
      //Then autofail without checking again.
      if ( failedPrims.find(it->first) != failedPrims.end() && !found ) {
	continue;
      }

      if ( onlyOnce && ( ldips.find(it->second->second.first) != ldips.end() ||
			 rdips.find(it->second->second.second) != rdips.end() ) ) continue;

      //Try to add the interaction.
      if (addInteraction(it->second, lrs, rrs, interactions, b, xSec)) {
	found = true;
	ldips.insert(it->second->second.first);
	rdips.insert(it->second->second.second);
	maxpt = max(maxpt, it->first);
	maxpte = max(maxpte, max(max(it->second->second.first->partons().first->pT().pt(),
				     it->second->second.first->partons().second->pT().pt()),
				 max(it->second->second.second->partons().first->pT().pt(),
				     it->second->second.second->partons().second->pT().pt()))/GeV);

      }
      else {
	//remember if it was first interaction and failed, to not
	//try it again later.
	if ( !found ) {
	  failedPrims.insert(*it);
	  if ( it == ordered.begin() ) {
	    ordered.erase(it);
	    sel.erase(it->second);
	  }
	}
      }
    }

    //if sel (or ordered) empty, give up event. no int can be first.
    if ( sel.empty() ) return make_pair(lrs, rrs);
  }

  //Make sure that the real state partons are on-shell.
  //May not actually be needed, not sure... TODO: check!
  for( DipolePairVector::iterator it = interactions.begin();
      it != interactions.end(); it++ ) {
    lrs->setOnShell((*it)->second.first);
    rrs->setOnShell((*it)->second.second);
  }
  for ( RealParton::RealPartonSet::iterator it = lrs->valence.begin();
	it != lrs->valence.end(); it++ )
    (*it)->setOnShell();
  for ( RealParton::RealPartonSet::iterator it = rrs->valence.begin();
	it != rrs->valence.end(); it++ )
    (*it)->setOnShell();

  // *** TO REMOVE *** Temporary analysis
  if ( histptmax ) {
    sumwmaxpt += currentWeight;
  }

  if ( histdda ) {
    histdda->fill(lrs->interactions.size() + 0.5, currentWeight);
    histddp->fill(potential.size() + 0.5, currentWeight);
  }

  //return the real states.
  return make_pair(lrs, rrs);
}
pair<RealPartonStatePtr, RealPartonStatePtr>
EventFiller::selectInteractions(const InteractionList & intl,
				const DipoleXSec & xSec) const {

  InteractionVector interactions;

  if ( histddi ) histddi->fill(intl.size() + 0.5, currentWeight);

  double sumfij = 0.0; //the sum of the individual nonuntarised int probs
  double sumUP = 0.0;   //The sum of the individual unitarised int probs

  //Set up a selector, and a map sorted on pt of the interaction.
  Selector<InteractionList::const_iterator, double> sel;
  InteractionPTSet ordered;

  //Loop through all possible interactions and fill
  //@sel and @ordered.
  for ( InteractionList::const_iterator it = intl.begin(); it != intl.end(); ++it ) {

    //If no interaction probability, don't bother.
    if ( it->f2 <= 0.0 ) continue;

    //insert into the selector.
    sel.insert(it->uf2, it);

    sumfij += it->f2;
    sumUP += it->uf2;

    ordered.insert(it);
  }

  //interaction probability (for at least one interaction)
  //of the two states.
  double totalP = Current<DipoleEventHandler>()->xSecFn().unitarize(sumfij);

  //create the real states.
  RealPartonStatePtr rrs = new_ptr(RealPartonState());
  RealPartonStatePtr lrs = new_ptr(RealPartonState());

  //Add the valence partons (as they are always real) and save.
  lrs->addValence(intl.begin()->dips.first->dipoleState());
  rrs->addValence(intl.begin()->dips.second->dipoleState());
  lrs->saveState();
  rrs->saveState();

  InteractionPTSet potential;
  InteractionPTSet failedPrims;
  bool found = false;
  int counter = 0;
  while ( !found ) {
    potential.clear();
    interactions.clear();

    //select a first interaction (since there has to be at least one).
    InteractionList::const_iterator prim = sel[UseRandom::rnd()];

    //Go through the other interactions and check if they are also
    //interacting. A modified probability is used, to make up for the
    //bias introduced in selecting a primary interaction.
    for ( InteractionList::const_iterator it = intl.begin(); it != intl.end(); ++it )
      if ( it == prim || correctedProb(totalP,sumUP,it->uf2) > UseRandom::rnd() )
	potential.insert(it);

    //Keep track on how many times we have tried to find a consistent
    //set of interactions. Give up after 10 tries.
    counter++;
    if ( counter > 10 ) {
      overTenEvents += currentWeight;
      return make_pair(lrs, rrs);
    }

    //test all potetential interactions in order, skipping already failed dips.
    //failed first interactions inserted in failedprims,
    //and from sel and ordered if first in ordered.
    int i = 0;
    set<tDipolePtr> ldips, rdips;
    set< pair<tPartonPtr, tPartonPtr> > intpairs;
    for ( InteractionPTSet::iterator iit = potential.begin();
	  iit != potential.end(); ++iit ) {
      InteractionList::const_iterator it = *iit;
      i++;

      //Check first interaction, and already failed as first.
      //Then autofail without checking again.
      if ( failedPrims.find(it) != failedPrims.end() && !found ) {
	continue;
      }


      if ( onlyOnce && ( ldips.find(it->dips.first) != ldips.end() ||
			 rdips.find(it->dips.second) != rdips.end() ) ) continue;

      TransverseMomentum recoil = it->rec;
      it->norec = false;
      if ( onlyOnce && intpairs.find(it->ints) != intpairs.end() ) {
	recoil = TransverseMomentum();
	it->norec = true;
      }
      

      //Try to add the interaction.
      if ( addInteraction(it, lrs, rrs, recoil, interactions, xSec) ) {
	found = true;
	ldips.insert(it->dips.first);
	rdips.insert(it->dips.second);
	intpairs.insert(it->ints);
      }
      else {
	//remember if it was first interaction and failed, to not
	//try it again later.
	if ( !found ) {
	  failedPrims.insert(it);
	  if ( iit == ordered.begin() ) {
	    ordered.erase(it);
	    sel.erase(it);
	  }
	}
      }
    }

    //if sel (or ordered) empty, give up event. no int can be first.
    if ( sel.empty() ) return make_pair(lrs, rrs);
  }

  //Make sure that the real state partons are on-shell.
  //May not actually be needed, not sure... TODO: check!
  for( InteractionVector::iterator it = interactions.begin();
      it != interactions.end(); it++ ) {
    lrs->setOnShell((*it)->dips.first);
    rrs->setOnShell((*it)->dips.second);
  }
  for ( RealParton::RealPartonSet::iterator it = lrs->valence.begin();
	it != lrs->valence.end(); it++ )
    (*it)->setOnShell();
  for ( RealParton::RealPartonSet::iterator it = rrs->valence.begin();
	it != rrs->valence.end(); it++ )
    (*it)->setOnShell();

  // *** TO REMOVE *** Temporary analysis
  if ( histptmax ) {
    sumwmaxpt += currentWeight;
  }

  if ( histdda ) {
    histdda->fill(lrs->interactions.size() + 0.5, currentWeight);
    histddp->fill(potential.size() + 0.5, currentWeight);
  }

  //return the real states.
  return make_pair(lrs, rrs);
}

EventFiller::InteractionVector
EventFiller::selectShadowInteractions(const InteractionList & intl,
				      const DipoleXSec & xSec) const {
  InteractionVector interactions;

  if ( histddi ) histddi->fill(intl.size() + 0.5, currentWeight);

  tDipoleStatePtr dl = &intl.begin()->dips.first->dipoleState();
  tDipoleStatePtr dr = &intl.begin()->dips.second->dipoleState();

  double sumfij = 0.0; //the sum of the individual nonuntarised int probs
  double sumUP = 0.0;   //The sum of the individual unitarised int probs

  //Set up a selector, and a map sorted on pt of the interaction.
  Selector<InteractionList::const_iterator, double> sel;
  InteractionPTSet ordered;

  //Loop through all possible interactions and fill
  //@sel and @ordered.
  for ( InteractionList::const_iterator it = intl.begin(); it != intl.end(); ++it ) {

    //If no interaction probability, don't bother.
    if ( it->f2 <= 0.0 ) continue;

    //insert into the selector.
    sel.insert(it->uf2, it);

    sumfij += it->f2;
    sumUP += it->uf2;

    ordered.insert(it);
  }

  //interaction probability (for at least one interaction)
  //of the two states.
  double totalP = xSec.unitarize(sumfij);

  InteractionPTSet potential;
  InteractionPTSet failedPrims;
  bool found = false;
  int counter = 0;
  while ( !found ) {
    potential.clear();
    interactions.clear();

    dl->resetShadows();
    dr->resetShadows();

    //select a first interaction (since there has to be at least one).
    InteractionList::const_iterator prim = sel[UseRandom::rnd()];

    //Go through the other interactions and check if they are also
    //interacting. A modified probability is used, to make up for the
    //bias introduced in selecting a primary interaction.
    for ( InteractionList::const_iterator it = intl.begin(); it != intl.end(); ++it )
      if ( it == prim || correctedProb(totalP,sumUP,it->uf2) > UseRandom::rnd() ) {
	if ( histff ) histff->fill(it->uf2, currentWeight);	
	potential.insert(it);
      }

    //Keep track on how many times we have tried to find a consistent
    //set of interactions. Give up after 10 tries.
    counter++;
    if ( counter > 10 ) {
      overTenEvents += currentWeight;
      return interactions;
    }

    // Test all potetential interactions in order, skipping already
    // failed dips.  Failed first interactions are inserted in
    // failedprims, and removed from sel and ordered if first in
    // ordered. Keep track of each dipole that has interacted, as a
    // dipole can only interact once. Also keep track of all pairs of
    // partons that has itneracted, as if the same paie is
    // rescattered, they should not be given double recoil.
    int i = 0;
    int Nc = potential.size() - 1;
    int Ncfp0 = 0;
    int Ncfk0 = 0;
    int Ncfo0 = 0;
    int Ncfp = 0;
    int Ncfk = 0;
    int Ncfo = 0;
    int Nca = 0;
    set<tDipolePtr> ldips, rdips;
    set< pair<tSPartonPtr, tSPartonPtr> > intpairs;

    for ( InteractionPTSet::iterator iit = potential.begin();
	  iit != potential.end(); ++iit ) {
      InteractionList::const_iterator it = *iit;
      i++;

      // Check first interaction, and if already failed as first
      // autofail it without checking again.
      if ( failedPrims.find(it) != failedPrims.end() && !found ) continue;

      // Never let the same dipole interact more than once.
      if ( ldips.find(it->dips.first) != ldips.end() ||
	   rdips.find(it->dips.second) != rdips.end() ) continue;

      // Try to add the interaction. First prepare by inserting the
      // interaction at a suitable ShadowParton.
      interactions.push_back(it);
      if ( i == 1  && it->check(-1) ) {
	cerr << "Failed in before prepare:" << endl;
	interactions[0]->debug();
	cerr << "Failed in before prepare:" << endl;
      }
	
      it->id = interactions.size();
      it->prepare();
      if ( i == 1  && it->check(-1) ) {
	cerr << "Failed in after prepare:" << endl;
	interactions[0]->debug();
      }
      // If the same two partons interact twice only give recoil once.
      it->norec = ( intpairs.find(it->sints) != intpairs.end() );
      // We need to check previously accepted interactions as well, so
      // we have to recheck all.
      if ( recheckInteractions(interactions, 0) ) {
	found = true;
	if ( histdd0 && interactions.size() > 1 ) ++Nca;
	ldips.insert(it->dips.first); // Prevent dipoles from interacting again.
	rdips.insert(it->dips.second);
	intpairs.clear(); // Re-build all pairs of interacting partons.
	for ( int i = 0, N = interactions.size(); i < N; ++i )
	  intpairs.insert(interactions[i]->sints);
      } else {
	if ( histdd0 && interactions.size() > 1 ) {
	  for ( int i = 0, N = interactions.size(); i < N; ++i ) {
	    if ( interactions[i]->status ) {
	      switch ( interactions[i]->status ) {
	      case DipoleInteraction::PROPFAIL:
		++( i < N - 1 ? Ncfp0: Ncfp );
		break;
	      case DipoleInteraction::KINEFAIL:
		++( i < N - 1 ? Ncfk0: Ncfk );
		break;
	      case DipoleInteraction::ORDERING:
		++( i < N - 1 ? Ncfo0: Ncfo );
		break;
	      case DipoleInteraction::UNKNOWN:
	      case DipoleInteraction::ACCEPTED:
		break;
	      }
	    } else
	      break;
	  }
	}
	interactions.pop_back();
	it->reject();
	it->id = -it->id;
	// *** TODO *** remove this - it's only for debugging.
	if ( !recheckInteractions(interactions, 0) )
	  Throw<ConsistencyException>()
	    << "In DIPSY::EventFiller: A previously accepted "
	    << "set of interactions was not accepted in "
	    << "subsequent debug check!" << Exception::abortnow;
	//remember if it was first interaction and failed, to not
	//try it again later.
	if ( !found ) {
	  failedPrims.insert(it);
	  if ( iit == ordered.begin() ) {
	    ordered.erase(it);
	    sel.erase(it);
	  }
	}
      }
    }

    if ( histdd0 && Nc ) {
      double nc = Nc;
      histdc0->fill(Nc + 0.5, currentWeight);
      histdcfp0->fill(Nc + 0.5, currentWeight*Ncfp0/nc);
      histdcfk0->fill(Nc + 0.5, currentWeight*Ncfk0/nc);
      histdcfo0->fill(Nc + 0.5, currentWeight*Ncfo0/nc);
      histdcfp->fill(Nc + 0.5, currentWeight*Ncfp/nc);
      histdcfk->fill(Nc + 0.5, currentWeight*Ncfk/nc);
      histdcfo->fill(Nc + 0.5, currentWeight*Ncfo/nc);
      histdca->fill(Nc + 0.5, currentWeight*Nca/nc);
    }

    //if sel (or ordered) empty, give up event. no int can be first.
    if ( sel.empty() ) return interactions;
  }

  // *** TO REMOVE *** Temporary analysis
  if ( histptmax ) {
    sumwmaxpt += currentWeight;
  }

  if ( histdda ) {
    histdda->fill(interactions.size() + 0.5, currentWeight);
    histddp->fill(potential.size() + 0.5, currentWeight);
    histddff->fill(xSec.nIBelowCut + 0.5, currentWeight);
    histddfp->fill(xSec.nIPropFail + 0.5, currentWeight);
    histddfk->fill(xSec.nIKineFail + 0.5, currentWeight);
    histddfo->fill(xSec.nIOrdering + 0.5, currentWeight);
  }

  return interactions;
}

bool EventFiller::
addInteraction(FList::const_iterator inter, RealPartonStatePtr lrs,
	       RealPartonStatePtr rrs, DipolePairVector & inters,
	       const ImpactParameters & b, const DipoleXSec & xSec) const {
    rescatter += currentWeight;

  //With some settings, only some of the partons actually interact.
  //Check which here. Other tunes will just return 4x true.
  pair<pair<bool, bool>, pair<bool, bool> > doesInt =
    xSec.doesInt(inter->second.first->partons(),
		 inter->second.second->partons(), b);

  if ( compat ) {
    pair<bool,bool> int0 =
       xSec.int0Partons(inter->second.first->partons().first,
			inter->second.first->partons().second,
			inter->second.second->partons().first,
			inter->second.second->partons().second, b);
    doesInt = make_pair(make_pair(int0.first, !int0.first),
			make_pair(int0.second, !int0.second));
  }

  //Calculate the recoils from the interaction.
  DipoleXSec::InteractionRecoil recs =
    xSec.recoil(inter->second.first->partons(),
		inter->second.second->partons(), b, doesInt);

  //Add the interacting partons and their parents to the real state
  //and go through the evolution checking for ordering, local pt-max, etc.
  if ( !lrs->fullControlEvolution
       (inter->second.first, inter->second.second,
	doesInt.first.first, doesInt.first.second,
	recs.first.first.pt(), recs.first.second.pt()) ) {
    return false;
  }

  //Add the interaction to the other state, and check that as well.
  if ( !rrs->fullControlEvolution
       (inter->second.second, inter->second.first,
	doesInt.second.first, doesInt.second.second,
	recs.second.first.pt(), recs.second.second.pt()) ) {
    //If this second evolution failed, remove the interaction from
    //the first
    //state by reverting it to the previously saved state.
    lrs->revertToPrevious(inter->second.first);

    return false;
  }

  //If both evolutions could accomodate the interacting partons,
  //add the interaction recoil as well, 
  //and check that things still are ok.
  inters.push_back(inter);
  if ( !controlRecoils(inters, lrs, rrs, b, xSec, doesInt) ) {
    //If failed, remove the interaction from the evolutions.
    lrs->revertToPrevious(inter->second.first);
    rrs->revertToPrevious(inter->second.second);
    inters.pop_back();

    return false;
  }

  //If the interactions was ok, mark the dipoles as interacting
  //and save the real states.
  xSec.interact(*inter->second.first, *inter->second.second);
  // inter->second.first->interact(*inter->second.second);
  // inter->second.second->interact(*inter->second.first);
  lrs->saveState();
  rrs->saveState();

  return true;
}
bool EventFiller::
addInteraction(InteractionList::const_iterator inter, RealPartonStatePtr lrs,
	       RealPartonStatePtr rrs, const TransverseMomentum & recoil,
	       InteractionVector & inters, const DipoleXSec & xSec) const {
    rescatter += currentWeight;

  //Add the interacting partons and their parents to the real state
  //and go through the evolution checking for ordering, local pt-max, etc.
  if ( !lrs->singleControlEvolution(inter->dips.first, inter->dips.second,
				  inter->ints.first, recoil) )
    return false;

  //Add the interaction to the other state, and check that as well.
  if ( !rrs->singleControlEvolution(inter->dips.second, inter->dips.first,
				  inter->ints.second, inter->b->invRotatePT(-recoil)) )
    return false;


  //If both evolutions could accomodate the interacting partons,
  //add the interaction recoil as well, 
  //and check that things still are ok.
  inters.push_back(inter);
  if ( !controlRecoils(inters, lrs, rrs, xSec) ) {
    //If failed, remove the interaction from the evolutions.
    lrs->revertToPrevious(inter->dips.first);
    rrs->revertToPrevious(inter->dips.second);
    inters.pop_back();

    return false;
  }

  //If the interactions was ok, mark the dipoles as interacting
  //and save the real states.
  xSec.interact(*inter->dips.first, *inter->dips.second);
  // inter->second.first->interact(*inter->second.second);
  // inter->second.second->interact(*inter->second.first);
  lrs->saveState();
  rrs->saveState();

  return true;
}


bool EventFiller::
recheckInteractions(const InteractionVector & interactions,
		    int mode) const {
  for ( int i = 0, N = interactions.size(); i < N; ++i ) {
    if ( i == 0 ) {
      interactions[0]->dips.first->dipoleState().resetInteractedShadows();
      interactions[0]->dips.second->dipoleState().resetInteractedShadows();
    }
    if ( interactions[i]->check(mode) ) {
      if ( i == 0 && N == 1 ) {
	cerr << "Failed in recheck:" << endl;
	interactions[i]->debug();
      }
      return false;
    }
  }
  return true;
}

vector<EventFiller::String>
EventFiller::extractStrings(DipoleState & dl, DipoleState & dr,
			    pair<RealPartonStatePtr, RealPartonStatePtr> realStates, 
                            const ImpactParameters & b ) const {

  //Just rename a bit for convenience.
  DipoleStatePtr rightState = &dr;
  DipoleStatePtr leftState = &dl;
  RealPartonStatePtr lrs = realStates.first;
  RealPartonStatePtr rrs = realStates.second;


  //grow back the partons marked as virtuals.
  //This will be the ones without interacting children, but not the
  //ones removed during ordering/pt-max-fixing.
  removeVirtuals(leftState);
  removeVirtuals(rightState);

  //The unordered and not-ok pt-max were removed by pairing them
  //up with other partons that should stay. Now merge all the momentum
  //into a single parton, and flag the others as virtual.
  lrs->mergeVirtuals();
  rrs->mergeVirtuals();
  //Save to update the parton information from the realPartons.
  lrs->saveState();
  rrs->saveState();

  //A high-pt parton (so very localised in x_T) can not recoil all
  //of a low-pt (so smeared out in x_T) parent, but will rather shoot
  //out a recoiling part of the parent. Fix this by replacing recoiled
  //low-pt parents by a remnant, and a recoiler.
  lrs->addRecoilers();
  rrs->addRecoilers();
  //Save states to update partons.
  lrs->saveState();
  rrs->saveState();

  //balance p+/p- by moving the entire state. Was first done in the 
  //interaction (where the interacting partons had to provide the 
  //energy), but the modifications here changes kinematics so that
  //some more balancing may be needed. Should very rarely be
  //large changes.
  fixBoost(lrs, rrs);

  //Translate the right state to it's right place in x_T,
  //and mirror it in rapidity so that it actually comes from the
  //other side. Finally merge the two cascades into one state.
  rightState->translate(b);
  rightState->mirror(0.0);
  DipoleStatePtr finalState = leftState->merge(rightState);

  vector<pair<DipolePtr, DipolePtr> > swinging =
    Current<DipoleEventHandler>()->xSecFn().getColourExchanges(lrs, rrs);

  //swing the interacting dipoles.
  for ( int i = 0; i < int(swinging.size()); i++ ) {
    Current<DipoleEventHandler>()->
      xSecFn().reconnect(swinging[i].first, swinging[i].second);
  }

  //now remove the partons that were made virtual by mergeVirtuals.
  //This is so that it should be easier to reconnect the colour
  //through the interacting dipoles with the state from the other side.
  removeVirtuals(finalState);

 //do final state swings, aka pythias colour reconnection.
  double yrange = FSSwingTime();
  double ystep = FSSwingTimeStep();
  for ( double y = 0.0; y < yrange; y += ystep ) {
    finalState->swingFS(y, y + ystep);
  }

  //  finalState->lambdaMeasure(0.36*GeV2, histDipLength2, histDipMass2);

  //compensate for the 6 valence charges in the proton
  finalState->normaliseValenceCharge(theValenceChargeNormalisation);

  //make sure the non-particpating nucleons (if AA) have the
  //original colour flow.
  finalState->restoreNonparticipants();

  //try to find and fix any problems or inconsistencies that may have
  //popped up. Error messages are sent if anything is found.
  dodgeErrors(finalState);

  //If you want to save the initial state event to file (mainly AA).
  //TODO: interface!
  //TODO: stop event after this.
  //finalState->saveGluonsToFile(currentWeight);

  //If you want to make a fancy movie in mathematica of your event. :)
  // finalState->printForMovie(0, 1000);

  //Sort the remaining partons in strings.
  vector<String> ret = finalState->strings();

  static DebugItem printfinal("DIPSY::PrintFinal", 6);

  if ( histptfi ) {
    double maxpt = 0.0;
    double maxpti = 0.0;
    for ( int i = 0, N = ret.size(); i < N; ++i )
      for ( int j = 0, M = ret[i].size(); j < M; ++j ) {
	double pt = ret[i][j]->pT().pt()/GeV;
	if ( abs(ret[i][j]->y()) < 1.0 ) {
	  histptf->fill(pt, currentWeight);
	  maxpt = max(maxpt, pt);
	  if ( ret[i][j]->interacted() ) {
	    histptfi->fill(ret[i][j]->pT().pt()/GeV, currentWeight);
	    maxpti = max(maxpti, pt);
	  }
	}
	if ( printfinal ) {
	  cerr << ( ret[i][j]->interacted()? "*": "-")
	       << ( ret[i][j]->valence()? "v": " ")
	       << ( ret[i][j]->rightMoving()? ">": "<")
	       << setw(9) << ret[i][j]->y()
	       << setw(10) << ret[i][j]->pT().pt()/GeV
	       << setw(10) << ret[i][j]->position().x()/InvGeV
	       << setw(10) << ret[i][j]->position().y()/InvGeV
	       << endl;
	}
	histptmax->fill(maxpt, currentWeight);
	histptmaxi->fill(maxpti, currentWeight);
      }
    if ( printfinal )  cerr << endl;
  }
  return ret;
}

vector<EventFiller::String>
EventFiller::extractShadowStrings(DipoleState & dl, DipoleState & dr,
			    const InteractionVector & interactions,
			    const ImpactParameters & b ) const {

  // First we check all interactions again, this time putting relevant
  // partons on shell.
  if ( !recheckInteractions(interactions, 1) )
    Throw<ConsistencyException>()
      << "In DIPSY::EventFiller: A previously accepted set of interactions was not "
      << "accepted in the final check!" << Exception::abortnow;

  // Translate the right state to it's right place in x_T,
  // and mirror it in rapidity so that it actually comes from the
  // other side. Finally merge the two cascades into one state.
  dr.translate(b);
  dr.mirror(0.0);
  DipoleStatePtr finalState = dl.merge(&dr);
  finalState->checkFSMomentum();

  //swing the interacting dipoles.
  for ( int i = 0, N = interactions.size(); i < N; i++ )
    Current<DipoleEventHandler>()->xSecFn().reconnect(*interactions[i]);
  finalState->checkFSMomentum();

  // Remove all virtual partons
  removeVirtuals(finalState);
  finalState->checkFSMomentum();

  // Compensate for the 6 valence charges in the proton
  finalState->normaliseValenceCharge(theValenceChargeNormalisation);
  finalState->checkFSMomentum();

  // Make sure the non-particpating nucleons (if AA) have the
  // original colour flow.
  finalState->restoreNonparticipants();
  finalState->checkFSMomentum();

  //try to find and fix any problems or inconsistencies that may have
  //popped up. Error messages are sent if anything is found.
  dodgeErrors(finalState);
  finalState->checkFSMomentum();

  //Sort the remaining partons in strings.
  vector<String> ret = finalState->strings();

  static DebugItem printfinal("DIPSY::PrintFinal", 6);

  if ( histptfi ) {
    for ( int i = 0, N = ret.size(); i < N; ++i )
      for ( int j = 0, M = ret[i].size(); j < M; ++j ) {
	if ( abs(ret[i][j]->y()) < 1.0 ) {
	  histptf->fill(ret[i][j]->pT().pt()/GeV, currentWeight);
	  if ( ret[i][j]->interacted() )
	    histptfi->fill(ret[i][j]->pT().pt()/GeV, currentWeight);
	}
	if ( printfinal ) {
	  cerr << ( ret[i][j]->interacted()? "*": "-")
	       << ( ret[i][j]->valence()? "v": " ")
	       << ( ret[i][j]->rightMoving()? ">": "<")
	       << setw(9) << ret[i][j]->y()
	       << setw(10) << ret[i][j]->pT().pt()/GeV
	       << setw(10) << ret[i][j]->position().x()/InvGeV
	       << setw(10) << ret[i][j]->position().y()/InvGeV
	       << endl;
	}
      }
    if ( printfinal )  cerr << endl;
  }
  return ret;
}

bool EventFiller::fillStep (Step & step, tPPair incoming,
			    const vector<String> & strings) const {
  Direction<0> dir(true);
  vector< vector<PPtr> > particles(strings.size());
  SubProPtr sub = new_ptr(SubProcess(incoming));
  for ( int is = 0, NS = strings.size(); is < NS; ++is ) {
    int N = strings[is].size();
    vector<bool> removed(N, false);
    if ( theSoftRemove && pT2Cut() > ZERO && N > 2 ) {
      bool changed = true;
      while ( changed ) {
	changed = false;
	for ( int i = 0; i < N; ++i ) {
	  if ( removed[i] ) continue;
	  if ( strings[is][i]->valence() && theSoftRemove == 2 ) continue;
	  if ( strings[is][i]->flavour() != ParticleID::g ) continue;
	  int i1 = (N + i - 1)%N;
	  while ( removed[i1] ) i1 = (N + i1 - 1)%N;
	  int i3 = (i + 1)%N;
	  while ( removed[i3] ) i3 = (i3 + 1)%N;
	  if ( i1 == i3 ) continue;
	  if ( invPT2(strings[is], i1, i, i3) < pT2Cut() ) {
	    if ( removeGluon(strings[is], i1, i, i3) ) {
	      removed[i] = changed = true;
	    } else {
	      return false;
	    }
	  }
	}
      }
    }	
    particles[is].resize(N);
    for ( int i = 0; i < N; ++i ) {
      if ( removed[i] ) continue;
      particles[is][i] = strings[is][i]->produceParticle();
      int ip = i - 1;
      while ( ip >= 0 && removed[ip] ) --ip;
      if ( ip >= 0 )
	particles[is][i]->colourConnect(particles[is][ip]);
      if ( i == N - 1 && strings[is][i]->flavour() == ParticleID::g ) {
	int i0 = 0;
	while ( i0 < i && removed[i0] ) ++i0;
	particles[is][i0]->colourConnect(particles[is][i]);
      }
      sub->addOutgoing(particles[is][i]);
    }
    if ( strings[is][0]->flavour() == ParticleID::g ) {
      int i0 = 0;
      int in = N - 1;
      while ( i0 < in && removed[i0] ) ++i0;
      while ( in > i0 && removed[in] ) --in;
      particles[is][i0]->colourConnect(particles[is][in]);
    }
  }

  step.addSubProcess(sub);

  return true;
}

void EventFiller::fixBoost(RealPartonStatePtr lrs, RealPartonStatePtr rrs) const {
  //access the CoM energy, through a kindof roundabout path...
  //TODO: must be a better way! Current?
  PartonPtr dummy = lrs->partons.begin()->first;
  Energy sqrtS = ((dummy->dipoles().first) ? dummy->dipoles().first:dummy->dipoles().second)->dipoleState().handler().lumiFn().maximumCMEnergy();

  //sum left and right p+ and p-.
  Energy leftPlus = ZERO;
  Energy leftMinus = ZERO;
  Energy rightPlus = ZERO;
  Energy rightMinus = ZERO;

  //loop and sum over the to-keep partons.
  for ( map<tPartonPtr,RealPartonPtr>::const_iterator
	  it =lrs->partons.begin();
	it != lrs->partons.end(); it++ ) {
    if ( it->second->keep == RealParton::NO ) continue;
    leftPlus += it->second->plus;
    leftMinus += it->second->minus;
  }
  for ( map<tPartonPtr,RealPartonPtr>::const_iterator
	  it = rrs->partons.begin();
	  it != rrs->partons.end(); it++ ) {
    if ( it->second->keep == RealParton::NO ) continue;
    rightPlus += it->second->minus;
    rightMinus += it->second->plus;
  }

  //Solve the 2:nd degree equation for how much energy has to be
  //transfered to set both states on shell.
  double A = (- rightPlus*rightMinus - sqr(sqrtS) + leftPlus*leftMinus)/(2.0*rightMinus*sqrtS);
  double B = rightPlus/rightMinus;
  double y1 = -1.0;
  double x1 = -1.0;
  if ( sqr(A) - B > 0.0 ) {
    //the factor to change right p- with.
    y1 = - A + sqrt(sqr(A) - B);
    // double y2 = - A - sqrt(sqr(A) - B);

    //The factor to change left p+ with.
    x1 = (y1*sqrtS - rightPlus)/(y1*leftPlus);
    // double x2 = (y2*sqrtS - rightPlus)/(y2*leftPlus);
  }

  //error handling
  if ( x1 < 0 || y1 < 0 ) {
    Throw<SpaceLikeGluons>()
      << "EventFiller::fixBoost gave negative or nan solution to boost equation, "
      << "will not balance momentum.\n"
      << "This is probably caused by a rouge gluon being far too virtual.\n"
      << Exception::warning;
    return;
  }

  //loop again, scaling the p+ and p- according to the solution above.
  for (   map<tPartonPtr,RealPartonPtr>::const_iterator it = lrs->partons.begin();
	  it != lrs->partons.end(); it++) {
    if ( it->second->keep == RealParton::NO ) continue;
    it->second->plus *= x1;
    it->second->minus /= x1;
  }
  for (   map<tPartonPtr,RealPartonPtr>::const_iterator it = rrs->partons.begin();
	  it != rrs->partons.end(); it++) {
    if ( it->second->keep == RealParton::NO ) continue;
    it->second->minus /= y1;
    it->second->plus *= y1;
  }

  //Save state to transfer the changes to the partons.
  lrs->saveState();
  rrs->saveState();
}

// void EventFiller::fixValence(Step & step, DipoleState & dl, DipoleState & dr) const {
//   list<PartonPtr> alll = dl.getPartons();
//   set<tcPartonPtr> vall;
//   for ( list<PartonPtr>::iterator pit = alll.begin(); pit != alll.end(); ++pit )
//     if ( (**pit).valence() ) vall.insert(*pit);
//   list<PartonPtr> allr = dr.getPartons();
//   set<tcPartonPtr> valr;
//   for ( list<PartonPtr>::iterator pit = allr.begin(); pit != allr.end(); ++pit )
//     if ( (**pit).valence() ) valr.insert(*pit);
//   vector<PPtr> lval;
//   vector<PPtr> rval;
//   for ( ParticleSet::iterator pit = step.particles().begin();
// 	pit != step.particles().end(); ++pit ) {
//     tcPartonPtr p = ParticleInfo::getParton(**pit);
//     if ( !p ) continue;
//     if ( member(vall, p) ) lval.push_back(*pit);
//     else if ( member(valr, p) ) rval.push_back(*pit);
//   }

//   if ( UseRandom::rndbool() ) {
//     dl.fixValence(step, lval);
//     dr.fixValence(step, rval);
//   } else {
//     dr.fixValence(step, rval);
//     dl.fixValence(step, lval);
//   }

// }

void EventFiller::dodgeErrors(DipoleStatePtr finalState) const {
  list<PartonPtr> partons = finalState->getPartons();
  for ( list<PartonPtr>::iterator it = partons.begin(); it != partons.end(); it++ ) {
    PartonPtr p = *it;
    //check for empty pointers
    if ( !p ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found empty pointer from getPartons()! :o" << Exception::warning;
      continue;
    }
    //check for colour flow to itself
    //check for 0 monetum
    if ( p->pT().pt() == ZERO ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found 0 pt gluon." << Exception::warning;
      p->pT(TransverseMomentum(UseRandom::rnd()*GeV,UseRandom::rnd()*GeV));
    }
    if ( p->plus() == ZERO || p->minus() == ZERO ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found 0 lightcone momentum." << Exception::warning;
      p->minus(UseRandom::rnd()*GeV);
      p->plus(UseRandom::rnd()*GeV);
    }

    //check for nan momentum
    if ( isnan(p->pT().pt()/GeV) ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found NAN pt gluon, fix pt." << Exception::warning;
      p->pT(TransverseMomentum(UseRandom::rnd()*GeV,UseRandom::rnd()*GeV));
    }
    if ( isnan(p->plus()/GeV) || isnan(p->minus()/GeV) ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found NAN lightcone momentum, fix plus, minus." << Exception::warning;
      p->minus(UseRandom::rnd()*GeV);
      p->plus(UseRandom::rnd()*GeV);
    }

    //check for negative momentum
    if ( p->pT().pt() < ZERO ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found negative pt gluon.... >_>, fix pt." << Exception::warning;
      p->pT(TransverseMomentum(UseRandom::rnd()*GeV,UseRandom::rnd()*GeV));
    }
    if ( p->plus() < ZERO || p->minus() < ZERO ) {
      Throw<SpaceLikeGluons>()
	<< "dodgeError found negative lightcone momentum, fix plus,minus." << Exception::warning;
      p->minus(UseRandom::rnd()*GeV);
      p->plus(UseRandom::rnd()*GeV);

      //check for off-shell momentum
      if ( sqr(p->plus()*p->minus() - p->pT().pt2() - sqr(p->mass())) > sqr(sqr(0.01*GeV)) ) {
	Throw<SpaceLikeGluons>()
	  << "dodgeError found off-shell parton, fix pt." << Exception::warning;
	if ( p->plus()*p->minus() < sqr(p->mass()) ) {
	  Throw<SpaceLikeGluons>()
	    << "dodgeError found insufficient energy for mass, fix plus,minus." << Exception::warning;
	  double ratio = 2.0*sqr(p->mass())/(p->plus()*p->minus()); //give a bit extra for pt
	  p->plus(p->plus()*sqrt(ratio));
	  p->minus(p->minus()*sqrt(ratio));
	}
	double mod = sqrt(p->plus()*p->minus() - sqr(p->mass()))/p->pT().pt();
	p->pT(p->pT()*mod);
      }
    }
  }
}

bool EventFiller::
controlRecoils(DipolePairVector & sel,
	       RealPartonStatePtr lrs, RealPartonStatePtr rrs,
	       const ImpactParameters & b, const DipoleXSec & xSec,
	       pair<pair<bool, bool>, pair<bool, bool> > doesInt) const {

  //Keep track on which partons are interacting.
  list<pair<bool, bool> >::const_iterator
    leftDoesInt = lrs->doesInts.begin();
  list<pair<bool, bool> >::const_iterator
    rightDoesInt = rrs->doesInts.begin();

  //Loop through the interaction, and
  for ( DipolePairVector::const_iterator inter = sel.begin();
	inter != sel.end(); inter++ ) {

    //Which partons are interacting in this dip-dip interaction
    pair<pair<bool, bool>, pair<bool, bool> >
      trueDoesInt = make_pair(*leftDoesInt++, *rightDoesInt++);
    //calculate recoils
    DipoleXSec::InteractionRecoil recoil =
      xSec.recoil((*inter)->second.first->partons(),
		  (*inter)->second.second->partons(), b, trueDoesInt);
    

    //call the DipoleXSec object to check kinematics and vetos
    //for the interaction. If things don't work, undo the changes.
    if ( !xSec.doInteraction(recoil, *inter, lrs, rrs, trueDoesInt, b) ) {
      //revert the total recoil between the states.
      lrs->totalRecoil -= recoil.first.first + recoil.first.second;
      rrs->totalRecoil -= recoil.second.first + recoil.second.second;
    }
  }
  return true;
}

bool EventFiller::
controlRecoils(InteractionVector & sel, RealPartonStatePtr lrs,
	       RealPartonStatePtr rrs,  const DipoleXSec & xSec) const {

  //Keep track on which partons are interacting.
  list<pair<bool, bool> >::const_iterator
    leftDoesInt = lrs->doesInts.begin();
  list<pair<bool, bool> >::const_iterator
    rightDoesInt = rrs->doesInts.begin();

  //Loop through the interaction, and
  for ( InteractionVector::const_iterator inter = sel.begin();
	inter != sel.end(); inter++ ) {

    //Which partons are interacting in this dip-dip interaction
    pair<pair<bool, bool>, pair<bool, bool> >
      trueDoesInt = make_pair(*leftDoesInt, *rightDoesInt);

    //calculate recoils
    DipoleXSec::InteractionRecoil recoil = xSec.recoil(**inter);

    //call the DipoleXSec object to check kinematics and vetos
    //for the interaction. If things don't work, undo the changes.
  if ( !xSec.doInteraction(recoil, *inter, lrs, rrs, trueDoesInt) ) {
      //revert the total recoil between the states.
      lrs->totalRecoil -= recoil.first.first + recoil.first.second;
      rrs->totalRecoil -= recoil.second.first + recoil.second.second;
    }
  }
  return true;
}

void EventFiller::removeVirtuals(DipoleStatePtr state) const {

  list<PartonPtr> vP = state->getPartons();

  //loop through the partons, and remove the off shell ones.
  //this only does colour flow. RealPartonState does the kinematics
  //in mergeVirtuals.
  for ( list<PartonPtr>::iterator it = vP.begin(); it != vP.end(); it++) {
    tPartonPtr p = *it;
    //only handle off-shell parton.
    if ( !(p->onShell()) ) {
      //if there are only 2 or fewer on-shell partons in the colour chain,
      //it has to be swinged at some point, and better early than late,
      //as last-second forced swings can lead to silly colour connections.
      if ( p->nOnShellInChain() < 2 ) {
	//tell the absorber to find a swing anywhere in the colour chain
	if ( p->dipoles().first ) absorber()->swingLoop(p->dipoles().first, *state);
	else absorber()->swingLoop(p->dipoles().second, *state);
      }
      
      //once off-shell loops are handled, absorb the parton.
      absorber()->removeParton(p);
    }
  }
}

// If needed, insert default implementations of virtual function defined
// in the InterfacedBase class here (using ThePEG-interfaced-impl in Emacs).


void EventFiller::doinit() throw(InitException) {
  HandlerBase::doinit();
}

void EventFiller::dofinish() {
  HandlerBase::dofinish();
  if ( sumwmaxpt > 0.0 ) {
    histptmax->scale(1.0/sumwmaxpt);
    histptmaxi->scale(1.0/sumwmaxpt);
    histptf->scale(1.0/sumwmaxpt);
    histptfi->scale(1.0/sumwmaxpt);
    histdd0->scale(0.01/sumwmaxpt);
    histddi->scale(0.01/sumwmaxpt);
    histdda->scale(1.0/sumwmaxpt);
    histddp->scale(1.0/sumwmaxpt);
    histddff->scale(0.01/sumwmaxpt);
    histddfp->scale(0.01/sumwmaxpt);
    histddfk->scale(0.01/sumwmaxpt);
    histddfo->scale(0.01/sumwmaxpt);
    histddra->scale(0.01/sumwmaxpt);
    histddrf->scale(0.01/sumwmaxpt);
    histddrp->scale(0.01/sumwmaxpt);
    histddrk->scale(0.01/sumwmaxpt);
    histddro->scale(0.01/sumwmaxpt);
    histdc0->scale(1.0/sumwmaxpt);
    histdcfp0->scale(1.0/sumwmaxpt);
    histdcfk0->scale(1.0/sumwmaxpt);
    histdcfo0->scale(1.0/sumwmaxpt);
    histdcfp->scale(1.0/sumwmaxpt);
    histdcfk->scale(1.0/sumwmaxpt);
    histdcfo->scale(1.0/sumwmaxpt);
    histdca->scale(1.0/sumwmaxpt);
    histf0->scale(100.0/sumwmaxpt);
    histfa->scale(100.0/sumwmaxpt);
    histff->scale(100.0/sumwmaxpt);
    histfp->scale(100.0/sumwmaxpt);
    histfk->scale(100.0/sumwmaxpt);
    histfo->scale(100.0/sumwmaxpt);
    double nd = DipoleInteraction::ofail[10] + DipoleInteraction::ofail[11] +
      DipoleInteraction::ofail[12] + DipoleInteraction::ofail[13];

    double n1d = DipoleInteraction::o1fail[10] + DipoleInteraction::o1fail[11]
      + DipoleInteraction::o1fail[12] + DipoleInteraction::o1fail[13];
    generator()->log()
      << endl
      << "Failed ordering in dipole--dipole interactions:" << endl
      << "  total : " << nd << '\t' << n1d << endl
      << "  cL-l  : " << DipoleInteraction::ofail[1]/nd << '\t'
      << DipoleInteraction::o1fail[1]/n1d << endl
      << "  aL-l  : " << DipoleInteraction::ofail[2]/nd << '\t'
      << DipoleInteraction::o1fail[2]/n1d << endl
      << "  cR-r  : " << DipoleInteraction::ofail[3]/nd << '\t'
      << DipoleInteraction::o1fail[3]/n1d << endl
      << "  aR-r  : " << DipoleInteraction::ofail[4]/nd << '\t'
      << DipoleInteraction::o1fail[4]/n1d << endl
      << "  cL-aR : " << DipoleInteraction::ofail[5]/nd << '\t'
      << DipoleInteraction::o1fail[5]/n1d << endl
      << "  cL-cr : " << DipoleInteraction::ofail[6]/nd << '\t'
      << DipoleInteraction::o1fail[6]/n1d << endl
      << "  cR-cl : " << DipoleInteraction::ofail[7]/nd << '\t'
      << DipoleInteraction::o1fail[7]/n1d << endl
      << "  aL-ar : " << DipoleInteraction::ofail[8]/nd << '\t'
      << DipoleInteraction::o1fail[8]/n1d << endl
      << "  aR-al : " << DipoleInteraction::ofail[9]/nd << '\t'
      << DipoleInteraction::o1fail[9]/n1d << endl
      << "    ncc : " <<  DipoleInteraction::ofail[10] << '\t'
      << DipoleInteraction::o1fail[10] << endl
      << "    nca : " <<  DipoleInteraction::ofail[11] << '\t'
      << DipoleInteraction::o1fail[11] << endl
      << "    nac : " <<  DipoleInteraction::ofail[12] << '\t'
      << DipoleInteraction::o1fail[12] << endl
      << "    naa : " <<  DipoleInteraction::ofail[13] << '\t'
      << DipoleInteraction::o1fail[13] << endl
      << "   ncc0 : " <<  DipoleInteraction::ofail[14] << '\t'
      << DipoleInteraction::o1fail[14] << endl
      << "   nca0 : " <<  DipoleInteraction::ofail[15] << '\t'
      << DipoleInteraction::o1fail[15] << endl
      << "   nac0 : " <<  DipoleInteraction::ofail[16] << '\t'
      << DipoleInteraction::o1fail[16] << endl
      << "   naa0 : " <<  DipoleInteraction::ofail[17] << '\t'
      << DipoleInteraction::o1fail[17] << endl
      << endl;

  }
}

void EventFiller::doinitrun() {
  HandlerBase::doinitrun();
  histptmax = FactoryBase::tH1DPtr();
  histptmaxi = FactoryBase::tH1DPtr();
  histptf = FactoryBase::tH1DPtr();
  histptfi = FactoryBase::tH1DPtr();
  histdd0 = FactoryBase::tH1DPtr();
  histddi = FactoryBase::tH1DPtr();
  histdda = FactoryBase::tH1DPtr();
  histddp = FactoryBase::tH1DPtr();
  histddff = FactoryBase::tH1DPtr();
  histddfp = FactoryBase::tH1DPtr();
  histddfk = FactoryBase::tH1DPtr();
  histddfo = FactoryBase::tH1DPtr();
  histddra = FactoryBase::tH1DPtr();
  histddrf = FactoryBase::tH1DPtr();
  histddrp = FactoryBase::tH1DPtr();
  histddrk = FactoryBase::tH1DPtr();
  histddro = FactoryBase::tH1DPtr();
  histdc0 = FactoryBase::tH1DPtr();
  histdcfp0 = FactoryBase::tH1DPtr();
  histdcfk0 = FactoryBase::tH1DPtr();
  histdcfo0 = FactoryBase::tH1DPtr();
  histdcfp = FactoryBase::tH1DPtr();
  histdcfk = FactoryBase::tH1DPtr();
  histdcfo= FactoryBase::tH1DPtr();
  histdca = FactoryBase::tH1DPtr();
  histf0 = FactoryBase::tH1DPtr();
  histfa = FactoryBase::tH1DPtr();
  histff = FactoryBase::tH1DPtr();
  histfp = FactoryBase::tH1DPtr();
  histfk = FactoryBase::tH1DPtr();
  histfo = FactoryBase::tH1DPtr();
  sumwmaxpt = 0.0;
  if ( debughistos && generator()->histogramFactory() ) {
    generator()->histogramFactory()->initrun();
    generator()->histogramFactory()->registerClient(this);
    histptmax = generator()->histogramFactory()->createHistogram1D
      ("ptma",100,0.0,100.0);
    histptmaxi = generator()->histogramFactory()->createHistogram1D
      ("ptmi",100,0.0,100.0);
    histptf = generator()->histogramFactory()->createHistogram1D
      ("ptfa",100,0.0,100.0);
    histptfi = generator()->histogramFactory()->createHistogram1D
      ("ptfi",100,0.0,100.0);
    histdd0 = generator()->histogramFactory()->createHistogram1D
      ("pdd0",200,0.0,20000.0);
    histddi = generator()->histogramFactory()->createHistogram1D
      ("pddi",200,0.0,20000.0);
    histdda= generator()->histogramFactory()->createHistogram1D
      ("pdda",100,0.0,100.0);
    histddp= generator()->histogramFactory()->createHistogram1D
      ("pddp",100,0.0,100.0);
    histddff= generator()->histogramFactory()->createHistogram1D
      ("pddff",200,0.0,20000.0);
    histddfp= generator()->histogramFactory()->createHistogram1D
      ("pddfp",200,0.0,20000.0);
    histddfk= generator()->histogramFactory()->createHistogram1D
      ("pddfk",200,0.0,20000.0);
    histddfo= generator()->histogramFactory()->createHistogram1D
      ("pddfo",200,0.0,20000.0);
    histddra= generator()->histogramFactory()->createHistogram1D
      ("pddra",200,0.0,20000.0);
    histddrf= generator()->histogramFactory()->createHistogram1D
      ("pddrf",200,0.0,20000.0);
    histddrp= generator()->histogramFactory()->createHistogram1D
      ("pddrp",200,0.0,20000.0);
    histddrk= generator()->histogramFactory()->createHistogram1D
      ("pddrk",200,0.0,20000.0);
    histddro= generator()->histogramFactory()->createHistogram1D
      ("pddro",200,0.0,20000.0);
    histdc0= generator()->histogramFactory()->createHistogram1D
      ("dc0",100,0.0,100.0);
    histdcfp0= generator()->histogramFactory()->createHistogram1D
      ("dcfp0",100,0.0,100.0);
    histdcfk0= generator()->histogramFactory()->createHistogram1D
      ("dcfk0",100,0.0,100.0);
    histdcfo0= generator()->histogramFactory()->createHistogram1D
      ("dcfo0",100,0.0,100.0);
    histdcfp= generator()->histogramFactory()->createHistogram1D
      ("dcfp",100,0.0,100.0);
    histdcfk= generator()->histogramFactory()->createHistogram1D
      ("dcfk",100,0.0,100.0);
    histdcfo= generator()->histogramFactory()->createHistogram1D
      ("dcfo",100,0.0,100.0);
    histdca= generator()->histogramFactory()->createHistogram1D
      ("dca",100,0.0,100.0);
    histf0 = generator()->histogramFactory()->createHistogram1D
      ("f0",100,0.0,1.0);
    histfa = generator()->histogramFactory()->createHistogram1D
      ("fa",100,0.0,1.0);
    histff = generator()->histogramFactory()->createHistogram1D
      ("ff",100,0.0,1.0);
    histfp = generator()->histogramFactory()->createHistogram1D
      ("fp",100,0.0,1.0);
    histfk = generator()->histogramFactory()->createHistogram1D
      ("fk",100,0.0,1.0);
    histfo = generator()->histogramFactory()->createHistogram1D
      ("fo",100,0.0,1.0);
  }
}

double EventFiller::pTScale(DipoleState & state) const {
  return state.handler().emitter().pTScale();
}

Energy2 EventFiller::invPT2(const String & str, int i1, int i2, int i3) {
  LorentzMomentum p1 = str[i1]->momentum();
  LorentzMomentum p2 = str[i2]->momentum();
  LorentzMomentum p3 = str[i3]->momentum();
  Energy2 s = (p1 + p2 + p3).m2();
  if ( s < sqr(str[i1]->mass() + str[i3]->mass() ) )
    Throw<SpaceLikeGluons>()
      << "DIPSY produced space-like gluons. Three neighboring ones had "
      << "negative mass squared. This cannot be fixed at the moment. "
      << "Event discarded." << Exception::eventerror;
  Energy2 s12 = (p1 + p2).m2();
  Energy2 s23 = (p2 + p3).m2();
  if ( s12 < ZERO || s23 < ZERO ) return -1.0*GeV2;
  return s12*s23/s;
}

bool EventFiller::removeGluon(const String & str, int i1, int i2, int i3) {
  LorentzMomentum p1 = str[i1]->momentum();
  LorentzMomentum p2 = str[i2]->momentum();
  LorentzMomentum p3 = str[i3]->momentum();
  Sum20Momentum sum20;
  sum20 << p1 << p2 << p3;
  LorentzMomentum ptest = p1 + p2;
  if ( ptest.m2() < Constants::epsilon*1000.0*sqr(ptest.e()) ) {
    str[i1]->plus(ptest.plus());
    str[i1]->pT(TransverseMomentum(ptest.x(), ptest.y()));
    return true;
  }
  ptest = p3 + p2;
  if ( ptest.m2() < Constants::epsilon*1000.0*sqr(ptest.e()) ) {
    str[i3]->plus(ptest.plus());
    str[i3]->pT(TransverseMomentum(ptest.x(), ptest.y()));
    return true;
  }

  Energy2 S = (p1 + p2 + p3).m2();
  if ( S <= ZERO ) return false;
  LorentzRotation R = Utilities::boostToCM(makeTriplet(&p1, &p2, &p3));
  double Psi = Constants::pi - p3.theta();
  double beta = 0.0;
  Energy W = sqrt(S);
  double x1 = 2.0*p1.e()/W;
  double x3 = 2.0*p3.e()/W;
  bool g1 = str[i1]->flavour() == ParticleID::g;
  bool g3 = str[i3]->flavour() == ParticleID::g;
  if ( ( g1 && g3 ) || (!g1 && !g3 ) )
    beta = Psi*sqr(x3)/(sqr(x1) + sqr(x3)); // minimize pt
  else if ( g1 )
    beta = Psi;
  R.rotateY(-beta);
  R.invert();
  Lorentz5Momentum p1n(p1.m());
  Lorentz5Momentum p3n(p3.m());
  try {
    SimplePhaseSpace::CMS(p1n, p3n, S, 1.0, 0.0);
  } catch ( ImpossibleKinematics ) {
    return false;
  }
  p1n.transform(R);
  p3n.transform(R);
  str[i1]->plus(p1n.plus());
  str[i1]->pT(TransverseMomentum(p1n.x(), p1n.y()));
  str[i1]->minus(p1n.minus());
  str[i3]->plus(p3n.plus());
  str[i3]->pT(TransverseMomentum(p3n.x(), p3n.y()));
  str[i3]->minus(p3n.minus());

  static DebugItem checkkinematics("DIPSY::CheckKinematics", 6);
  if ( checkkinematics ) {
    sum20 >> p1n >> p3n;
    if ( !sum20 )
      Throw<ConsistencyException>()
     	<< "DIPSY found energy-momentum non-conservation when removing "
     	"gluon in final state."
     	<< Exception::warning;
  }
  return true;
}




void EventFiller::persistentOutput(PersistentOStream & os) const {
  os << theAbsorber << theRecoilScheme << theMode << theSingleMother
     << theDGLAPinPT << theEffectiveWeights << theFSSwingTime
     << theFSSwingTimeStep << theValenceChargeNormalisation
     << ounit(thePTCut, GeV) << theSoftRemove << onlyOnce << compat << debughistos;
}

void EventFiller::persistentInput(PersistentIStream & is, int) {
  is >> theAbsorber >> theRecoilScheme >> theMode >> theSingleMother
     >> theDGLAPinPT >> theEffectiveWeights >> theFSSwingTime
     >> theFSSwingTimeStep >> theValenceChargeNormalisation
     >> iunit(thePTCut, GeV) >> theSoftRemove >> onlyOnce >> compat >> debughistos;
}

DescribeClass<EventFiller,HandlerBase>
describeDIPSYEventFiller("DIPSY::EventFiller", "libAriadne5.so libDIPSY.so");

void EventFiller::Init() {

  static ClassDocumentation<EventFiller> documentation
    ("The EventFiller class is able to produce an initial ThePEG::Step "
     "from two colliding DipoleStates.");


  static Reference<EventFiller,DipoleAbsorber> interfaceDipoleAbsorber
    ("DipoleAbsorber",
     "The object used to absorb non-interacting dipoles.",
     &EventFiller::theAbsorber, true, false, true, true, false);

  static Switch<EventFiller,int> interfaceMode
    ("Mode",
     "How the real state is found from the virtual cascade. Speed versus consistency.",
     &EventFiller::theMode, 0, true, false);
  static SwitchOption interfaceModeMode
    (interfaceMode,
     "Consistent",
     "The fully consistent version. Not currently reccomended.",
     0);
  static SwitchOption interfaceModeFast
    (interfaceMode,
     "Fast",
     "Checking only the new real partons introduced by each new interaction, rather than rechecking them all. Good for heavy ions. Not currently reccomended.",
     1);
  static SwitchOption interfaceModeSingle
    (interfaceMode,
     "SingleSweep",
     "Checking all partons, but sweeping just once in each direction, making "
     "it a lot faster. Good for heavy ions. May give an occasional unordered "
     "chain, but hopefully not too often. The recommended option.",
     2);
  static SwitchOption interfaceModeNonRecursive
    (interfaceMode,
     "NonRecursive",
     "Does all evo no matter what. Then removes biggest problem, and does it "
     "all over again. Never turns partons back on once switched off.",
     3);
  static SwitchOption interfaceModeNewSingle
    (interfaceMode,
     "NewSingle",
     "A new implementation of SingleSweep.",
     4);

  static Switch<EventFiller,int> interfaceEffectiveWeights
    ("EffectiveWeights",
     "How the p+ and pT recoils are distributed among the single partons in an effective parton.",
     &EventFiller::theEffectiveWeights, 0, true, false);
  static SwitchOption interfaceEffectiveWeightsPlusWeighted
    (interfaceEffectiveWeights,
     "PlusWeighted",
     "Weight pt and plus according to p+ of the individual partons.",
     0);
  static SwitchOption interfaceEffectiveWeightsPlusEvenWeighted
    (interfaceEffectiveWeights,
     "PlusEvenWeighted",
     "The plus is distibuted according to the plus of the partons, but the pt is shared evenly among the partons.",
     1);
  static SwitchOption interfaceEffectiveWeightsPlusSingleWeighted
    (interfaceEffectiveWeights,
     "PlusSingleWeighted",
     "The plus is distibuted according to the plus of the partons, but the pt is taken only by the colour connected parton",
     2);

  static Switch<EventFiller,int> interfaceRecoilScheme
    ("RecoilScheme",
     "How to give tread the positive light-cone momentum of a recoiling parton.",
     &EventFiller::theRecoilScheme, 0, true, false);
  static SwitchOption interfaceRecoilSchemePreservePlus
    (interfaceRecoilScheme,
     "PreservePlus",
     "blaha",
     0);
  static SwitchOption interfaceRecoilSchemeFixedY
    (interfaceRecoilScheme,
     "FixedY",
     "blahaaa",
     1);
  static SwitchOption interfaceRecoilSchemeFrameFan
    (interfaceRecoilScheme,
     "FrameFan",
     "dssklajhls",
     2);

  static Parameter<EventFiller,int> interfaceSingleMother
    ("SingleMother",
     "If an emission is regarded to come from a single parton rather than both "
     " partons in the emitting dipole.",
     &EventFiller::theSingleMother, 1, 1, 0, 0,
     true, false, Interface::lowerlim);

  static Parameter<EventFiller,int> interfaceDGLAPinPT
    ("DGLAPinPT",
     "If the DGLAP supression should be made in terms of pt rather than r. "
     " partons in the emitting dipole.",
     &EventFiller::theDGLAPinPT, 1, 1, 0, 0,
     true, false, Interface::lowerlim);


  static Parameter<EventFiller,double> interfaceFSSwingTimeStep
    ("FSSwingTimeStep",
     "How long time steps is are to be used for FS colour reconnections.",
     &EventFiller::theFSSwingTimeStep, 0.1, 0.0, 0,
     true, false, Interface::lowerlim);


  static Parameter<EventFiller,double> interfaceFSSwingTime
    ("FSSwingTime",
     "How long time is allowed for FS colour reconnections. 0 turns it off.",
     &EventFiller::theFSSwingTime, 0.0, 0.0, 0,
     true, false, Interface::lowerlim);

  static Switch<EventFiller,int> interfaceValenceChargeNormalisation
    ("ValenceChargeNormalisation",
     "How to treat the (too large) colour charge of the valence partons.",
     &EventFiller::theValenceChargeNormalisation, 0, true, false);
  static SwitchOption interfaceValenceChargeNormalisationNone
    (interfaceValenceChargeNormalisation,
     "None",
     "Dont do anything.",
     0);
  static SwitchOption interfaceValenceChargeNormalisationSwing
    (interfaceValenceChargeNormalisation,
     "Swing",
     "Swing some of the dipoles going to the valence partons",
     1);

  static Parameter<EventFiller,Energy> interfacePTCut
    ("PTCut",
     "The minimum invariant transverse momentum allowed for a gluon. "
     "Gluons below the cut will be removed from the final state. "
     "If zero, no gluons will be removed.",
     &EventFiller::thePTCut, GeV, 0.0*GeV, 0.0*GeV, 0*GeV,
     true, false, Interface::lowerlim);

  static Switch<EventFiller,int> interfaceSoftRemove
    ("SoftRemove",
     "Determines if gluons with invariant transverse momentum below "
     "<interface>PTCut</interface> should be reabsorbed.",
     &EventFiller::theSoftRemove, 1, true, false);
  static SwitchOption interfaceSoftRemoveOff
    (interfaceSoftRemove,
     "Off",
     "No gluons are absorbed",
     0);
  static SwitchOption interfaceSoftRemoveAll
    (interfaceSoftRemove,
     "All",
     "All soft gluons below the cut are absorbed.",
     1);
  static SwitchOption interfaceSoftRemoveNoValence
    (interfaceSoftRemove,
     "NoValence",
     "All except valence gluons are absorbed if below the cut.",
     2);


  static Switch<EventFiller,bool> interfaceOnlyOnce
    ("OnlyOnce",
     "Do not allow a dipole to interact more than once.",
     &EventFiller::onlyOnce, false, true, false);
  static SwitchOption interfaceOnlyOnceManyInteractionsPerDipole
    (interfaceOnlyOnce,
     "ManyInteractionsPerDipole",
     "Allow a dipole to interact several times.",
     false);
  static SwitchOption interfaceOnlyOnceOneInteractionPerDipole
    (interfaceOnlyOnce,
     "OneInteractionPerDipole",
     "Only one interaction per dipole.",
     true);

  static Parameter<EventFiller,int> interfaceCompatMode
    ("CompatMode",
     "Compatibility mode for debugging differences between FList and InteractionList.",
     &EventFiller::compat, 0, 0, 0,
     true, false, Interface::lowerlim);

  static Parameter<EventFiller,int> interfaceDebugHist
    ("DebugHist",
     "Emit histograms for debugging.",
     &EventFiller::debughistos, 0, 0, 0,
     true, false, Interface::lowerlim);


}





