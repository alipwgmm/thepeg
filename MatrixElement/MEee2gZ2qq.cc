// -*- C++ -*-
//
// MEee2gZ2qq.cc is a part of ThePEG - Toolkit for HEP Event Generation
// Copyright (C) 1999-2017 Leif Lonnblad
//
// ThePEG is licenced under version 3 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
//
// This is the implementation of the non-inlined, non-templated member
// functions of the MEee2gZ2qq class.
//

#include "MEee2gZ2qq.h"
#include "ThePEG/Interface/ClassDocumentation.h"
#include "ThePEG/PDT/EnumParticles.h"
#include "ThePEG/Repository/EventGenerator.h"
#include "ThePEG/StandardModel/StandardModelBase.h"
#include "ThePEG/Handlers/StandardXComb.h"

#include "ThePEG/Persistency/PersistentOStream.h"
#include "ThePEG/Persistency/PersistentIStream.h"

using namespace ThePEG;

MEee2gZ2qq::MEee2gZ2qq()
  : coefs(20), mZ2(ZERO), GZ2(ZERO), lastCont(0.0), lastBW(0.0) {}

unsigned int MEee2gZ2qq::orderInAlphaS() const {
  return 0;
}

unsigned int MEee2gZ2qq::orderInAlphaEW() const {
  return 2;
}

void MEee2gZ2qq::getDiagrams() const {
  tcPDPtr gamma = getParticleData(ParticleID::gamma);
  tcPDPtr Z0 = getParticleData(ParticleID::Z0);
  tcPDPtr ep = getParticleData(ParticleID::eplus);
  tcPDPtr em = getParticleData(ParticleID::eminus);
  for ( int i = 1; i <= maxFlavour(); ++i ) {
    tcPDPtr q = getParticleData(i);
    tcPDPtr qb = q->CC();
    add(new_ptr((Tree2toNDiagram(2), em, ep, 1, gamma, 3, q, 3, qb, -1)));
    add(new_ptr((Tree2toNDiagram(2), em, ep, 1, Z0, 3, q, 3, qb, -2)));
  }
}

Energy2 MEee2gZ2qq::scale() const {
  return sHat();
}

double MEee2gZ2qq::me2() const {

  Energy2 m2 = meMomenta()[2].mass2();
  //  Energy2 p1p3 = 0.5*(m2 - tHat());
  //  Energy2 p1p2 = 0.5*sHat();
  Energy2 p1p3 = meMomenta()[0].dot(meMomenta()[2]);
  Energy2 p1p2 = meMomenta()[0].dot(meMomenta()[1]);
  Energy4 pt2 = sqr(p1p3);
  Energy4 pts = p1p3*p1p2;
  Energy4 ps2 = sqr(p1p2);
  Energy4 psm = p1p2*m2;

  int up = abs(mePartonData()[2]->id() + 1)%2;

  lastCont =
    (coefs[0 + up]*(pt2 - pts) + coefs[2 + up]*(ps2 + psm))/sqr(sHat());
  double intr = 0.25*(coefs[4 + up]*pt2 + coefs[6 + up]*pts +
		 coefs[8 + up]*ps2 + coefs[10 + up]*psm)*
    (sHat() - mZ2)/(sHat()*(sqr(sHat() - mZ2) + mZ2*GZ2));
  lastBW = 0.25*(coefs[12 + up]*pt2 + coefs[14 + up]*pts +
	    coefs[16 + up]*ps2 + coefs[18 + up]*psm)/
    (sqr(sHat() - mZ2) + mZ2*GZ2);

  double alphaS = SM().alphaS(scale());
  int Nf = SM().Nf(scale());
  DVector save;
  meInfo(save << lastCont << lastBW);
  return (lastCont + intr + lastBW)*sqr(SM().alphaEM(scale()))*
    (1.0 + alphaS/Constants::pi + (1.986-0.115*Nf)*sqr(alphaS/Constants::pi));
}

Selector<MEee2gZ2qq::DiagramIndex>
MEee2gZ2qq::diagrams(const DiagramVector & diags) const {
  if ( lastXCombPtr() ) {
    lastCont = meInfo()[0];
    lastBW = meInfo()[1];
  }
  Selector<DiagramIndex> sel;
  for ( DiagramIndex i = 0; i < diags.size(); ++i ) {
    if ( diags[i]->id() == -1 ) sel.insert(lastCont, i);
    else if ( diags[i]->id() == -2 ) sel.insert(lastBW, i);
  }
  return sel;
}

Selector<const ColourLines *>
MEee2gZ2qq::colourGeometries(tcDiagPtr) const {

  static ColourLines c("-5 4");

  Selector<const ColourLines *> sel;
  sel.insert(1.0, &c);
  return sel;
}

IBPtr MEee2gZ2qq::clone() const {
  return new_ptr(*this);
}

IBPtr MEee2gZ2qq::fullclone() const {
  return new_ptr(*this);
}

void MEee2gZ2qq::doinit() {
  double C = sqr(4.0*Constants::pi)/3.0;
  double SW2 = SM().sin2ThetaW();
  double SW4 = sqr(SW2);
  double SW6 = SW2*SW4;
  double SW8 = SW2*SW6;
  double CW2 = 1.0 - SW2;
  coefs[0] =  16.0*C;
  coefs[1] =  64.0*C;
  coefs[2] =   8.0*C;
  coefs[3] =  32.0*C;
  C /= (CW2*SW2);
  coefs[4] =  4.0*(32.0*SW4 - 32.0*SW2 +  6.0)*C;
  coefs[5] =  8.0*(64.0*SW4 - 40.0*SW2 +  6.0)*C;
  coefs[6] = -4.0*(32.0*SW4 - 32.0*SW2 + 12.0)*C;
  coefs[7] = -8.0*(64.0*SW4 - 40.0*SW2 + 12.0)*C;
  coefs[8] =  4.0*(16.0*SW4 - 16.0*SW2 +  6.0)*C;
  coefs[9] =  8.0*(32.0*SW4 - 20.0*SW2 +  6.0)*C;
  coefs[10] = 4.0*(16.0*SW4 - 16.0*SW2 +  3.0)*C;
  coefs[11] = 8.0*(32.0*SW4 - 20.0*SW2 +  3.0)*C;
  C /= (CW2*SW2);
  coefs[12] =  ( 64.0*SW8 - 128.0*SW6 + 128.0*SW4 -  48.0*SW2 +  9.0)*C;
  coefs[13] =  (256.0*SW8 - 320.0*SW6 + 200.0*SW4 -  60.0*SW2 +  9.0)*C;
  coefs[14] = -( 64.0*SW8 - 128.0*SW6 + 176.0*SW4 -  96.0*SW2 + 18.0)*C;
  coefs[15] = -(256.0*SW8 - 320.0*SW6 + 296.0*SW4 - 120.0*SW2 + 18.0)*C;
  coefs[16] =  ( 32.0*SW8 -  64.0*SW6 +  88.0*SW4 -  48.0*SW2 +  9.0)*C;
  coefs[17] =  (128.0*SW8 - 160.0*SW6 + 148.0*SW4 -  60.0*SW2 +  9.0)*C;
  coefs[18] =  ( 32.0*SW8 -  64.0*SW6 +  28.0*SW4 -   6.0*SW2)*C;
  coefs[19] =  (128.0*SW8 - 160.0*SW6 +  64.0*SW4 -  12.0*SW2)*C;

  tcPDPtr Z0 = getParticleData(ParticleID::Z0);
  mZ2 = sqr(Z0->mass());
  GZ2 = sqr(Z0->width());

  ME2to2QCD::doinit();
}

void MEee2gZ2qq::persistentOutput(PersistentOStream & os) const {
  os << coefs << ounit(mZ2, GeV2) << ounit(GZ2, GeV2) << lastCont << lastBW;
}

void MEee2gZ2qq::persistentInput(PersistentIStream & is, int) {
  is >> coefs >> iunit(mZ2, GeV2) >> iunit(GZ2, GeV2) >> lastCont >> lastBW;
}

ClassDescription<MEee2gZ2qq> MEee2gZ2qq::initMEee2gZ2qq;

void MEee2gZ2qq::Init() {

  static ClassDocumentation<MEee2gZ2qq> documentation
    ("The ThePEG::MEee2gZ2qq class implements the full"
     "\\f$e^+e^- \\rightarrow \\gamma/Z^0 \\rightarrow q\\bar{q}\\f$ "
     "matrix element including the interference terms.");

}

