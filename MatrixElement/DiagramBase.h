// -*- C++ -*-
//
// DiagramBase.h is a part of ThePEG - Toolkit for HEP Event Generation
// Copyright (C) 1999-2017 Leif Lonnblad
//
// ThePEG is licenced under version 3 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
#ifndef ThePEG_DiagramBase_H
#define ThePEG_DiagramBase_H
// This is the declaration of the DiagramBase class.

#include "ThePEG/Config/ThePEG.h"
#include "ThePEG/PDT/ParticleData.h"
#include "ThePEG/MatrixElement/ColourLines.h"
#include "ThePEG/Handlers/StandardXComb.fh"
#include "DiagramBase.fh"
#include "DiagramBase.xh"

namespace ThePEG {

/**
 * DiagramBase is the base class of all classes which describes
 * Feynman diagrams which can be generated by a matrix element class
 * inheriting from MEBase, as reported by the
 * MEBase::includedDiagrams() method.
 *
 * To work properly, a sub-class must in its constructor report the
 * incoming and outgoing parton types with the partons(int, const
 * cPDVector &, int) method. Also an id number should be given to be
 * used internally by the matrix element class. In addition, the
 * construct() method must be implemented to construct the actual
 * partons and connect them together in a SubProcess object, also
 * performing the colour connections using a given ColourLines object.
 *
 * @see MEBase
 * @see SubProcess
 * @see ColourLines
 * 
 */
class DiagramBase: public Base {

public:

  /** @name Standard constructors and destructors. */
  //@{
  /**
   * Default constructor.
   */
  DiagramBase() : theNIncoming(-1), theId(0) {}

  /**
   * Destructor.
   */
  virtual ~DiagramBase();
  //@}

public:

  /** @name Main virtual function to be overridden in sub-classes. */
  //@{
  /**
   * Construct a sub process corresponding to this diagram. The
   * incoming partons, and the momenta of the outgoing ones, are given
   * by the XComb object. All parent/children pointers should be set
   * correspondingly and the partons should be colour connected as
   * specified by the ColourLines object.
   */
  virtual tPVector construct(SubProPtr sb, const StandardXComb &,
			     const ColourLines &) const = 0;
  //@}
  
  /** @name Access the underlying information. */
  //@{
  /**
   * Return the number of incoming partons for this diagram. I.e. the
   * incoming partons plus the number of space-like lines.
   */
  int nIncoming() const { return theNIncoming; }

  /**
   * Return the incoming, followed by the outgoing partons for this
   * diagram.
   */
  const cPDVector& partons() const { return thePartons; }

  /**
   * Return the id number of this diagram.
   */
  int id() const { return theId; }

  /**
   * Generate a tag which is unique for diagrams with the same
   * type of incoming and outgoing partons.
   */
  string getTag() const;

  /**
   * Compare this diagram to another one modulo
   * the ids of the diagrams.
   */
  virtual bool isSame (tcDiagPtr other) const {
    return 
      nIncoming() == other->nIncoming() &&
      partons() == other->partons();
  }
  //@}

protected:

  /**
   * To be used by sub classes to report the incoming and outgoing
   * particle types, and an id number.
   *
   * @param ninc the number of incoming and other space-like lines in
   * the diagram.
   *
   * @param parts the types of partons for each external line in the
   * diagram.
   *
   * @param newId the id number of this diagram.
   */
  void partons(int ninc, const cPDVector & parts, int newId) {
    theNIncoming = ninc;
    thePartons = parts;
    theId = newId;
  }

  /**
   * Complete the missing information, provided partons() has already been
   * filled
   */
  void diagramInfo(int ninc, int newId) {
    theNIncoming = ninc;
    theId = newId;
  }

  /**
   * Returns true if the partons(int, const cPDVector &, int) function
   * has been called properly from the sub class.
   */
  bool done() const { return nIncoming() >= 0; }

  /**
   * Add to the partons
   */
  void addParton(tcPDPtr pd) { thePartons.push_back(pd); }

public:

  /** @name Functions used by the persistent I/O system. */
  //@{
  /**
   * Function used to write out object persistently.
   * @param os the persistent output stream written to.
   */
  void persistentOutput(PersistentOStream & os) const;

  /**
   * Function used to read in object persistently.
   * @param is the persistent input stream read from.
   * @param version the version number of the object when written.
   */
  void persistentInput(PersistentIStream & is, int version);
  //@}

  /**
   * Standard Init function.
   */
  static void Init();

private:

  /**
   * The number of incoming partons for this diagram.
   */
  int theNIncoming;

  /**
   * The incoming, followed by the outgoing partons for this
   * diagram.
   */
  cPDVector thePartons;

  /**
   * The id number of this diagram.
   */
  int theId;

private:

  /**
   * Describe an abstract base class with persistent data.
   */
  static AbstractClassDescription<DiagramBase> initDiagramBase;

  /**
   *  Private and non-existent assignment operator.
   */
  DiagramBase & operator=(const DiagramBase &);

};

}


namespace ThePEG {

/** @cond TRAITSPECIALIZATIONS */

/**
 * This template specialization informs ThePEG about the
 * base class of DiagramBase.
 */
template <>
struct BaseClassTrait<DiagramBase,1>: public ClassTraitsType {
  /** Typedef of the base class of DiagramBase. */
  typedef Base NthBase;
};

/**
 * This template specialization informs ThePEG about the name of the
 * DiagramBase class.
 */
template <>
struct ClassTraits<DiagramBase>: public ClassTraitsBase<DiagramBase> {
  /** Return the class name. */
  static string className() { return "ThePEG::DiagramBase"; }
};

/** @endcond */

}

#endif /* ThePEG_DiagramBase_H */
