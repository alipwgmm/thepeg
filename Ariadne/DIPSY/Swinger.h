// -*- C++ -*-
#ifndef DIPSY_Swinger_H
#define DIPSY_Swinger_H
//
// This is the declaration of the Swinger class.
//

#include "ThePEG/Handlers/HandlerBase.h"
#include "Parton.h"
#include "Swinger.fh"
#include "Dipole.fh"

namespace DIPSY {

using namespace ThePEG;

/**
 * The Swinger class is responsible for generating and performing a
 * swing for a dipole.  This base class does the standard thing. Any
 * non-standard thing must be implemented in a sub-class overriding
 * the generateRec() and/or recombine() functions.
 *
 * @see \ref SwingerInterfaces "The interfaces"
 * defined for Swinger.
 */
class Swinger: public HandlerBase {

public:

  /** @name Standard constructors and destructors. */
  //@{
  /**
   * The default constructor.
   */
  inline Swinger();

  /**
   * The copy constructor.
   */
  inline Swinger(const Swinger &);

  /**
   * The destructor.
   */
  virtual ~Swinger();
  //@}

public:

  /** @name Virtual functions which may be overridden by sub-classes. */
  //@{
  /**
   * Generate a swing for FS colour reconnection.
   */
  virtual void generateFS(Dipole & dipole, double miny, double maxy) const;

  /**
   * The FS amplitude for swinging, taking time and rapidity distance into account
   */
  virtual double swingAmpFS(const pair<tPartonPtr, tPartonPtr>,
			    const pair<tPartonPtr, tPartonPtr>,
			    InvEnergy2 time) const;

  /**
   * Calculate the distance between partons entering in the FS swing.
   */
  virtual InvEnergy2
  swingDistanceFS(const Parton & p1, const Parton & p2, InvEnergy2 time) const;

  /**
   * The FS amplitude for swinging, taking distances calculated by
   * swingDistanceFS().
   */
  virtual double
  swingAmpFS(InvEnergy2 a, InvEnergy2 b, InvEnergy2 c, InvEnergy2 d) const;

  /**
   * Generate a swing for the given \a dipole in the given rapidity
   * interval [\a miny,\a maxy]. If \a force is true, always generate
   * a swing, otherwise only check if a swing is possible with dipoles
   * which are new or has changed.
   */
  virtual void generate(Dipole & dipole,
			double miny, double maxy, bool force) const;

  /**
   * Try really hard to generate a swing. But if larger rapidity gap
   * than /a ymax, then don't bother. /a ymax = 0 means no limit.
   */
  virtual bool forceGenerate(Dipole & d, double ymax) const;

  /**
   * Perform a recombination previously generated by generateRec().
   */
  virtual void recombine(Dipole & dipole) const;

  /**
   * Perform a recombination previously generated by swingFS. Does not
   * produce new dipoles, just recombines the current ones.
   */
  virtual void recombineFS(Dipole & dipole) const;

  /**
   * The probability per rapidity for a swing to happen between the two dipoles.
   */
  virtual double swingAmp(const Dipole & d1, const Dipole & d2) const;

  /**
   * The probability per rapidity for a swing to happen between the two dipoles.
   */
  virtual double swingAmp(const pair<tPartonPtr, tPartonPtr>,
			  const pair<tPartonPtr, tPartonPtr>) const;

  /**
   * Tests the distribution in generated y - miny for a swing between two dipoles.
   */
  virtual void testGenerate(set<DipolePtr> & dips,double miny, double maxy);

  //inline functions

  /**
   * get lambda.
   */
  inline double lambda() const {
    return theLambda;
  }

  /**
   * set lambda.
   */
  inline void lambda(double x) {
    theLambda = x;
  }
  //@}

protected:

  /**
   * Checks that a swing doesnt push any parton above maxy, using the pT
   * definition that depends on the two colour neighbours.
   * Internal function.
   */
  virtual bool checkMaxY(DipolePtr dip, DipolePtr swingDip, double maxY ) const;

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
   * The standard Init function used to initialize the interfaces.
   * Called exactly once for each class by the class description system
   * before the main function starts or
   * when this class is dynamically loaded.
   */
  static void Init();



protected:

  /** @name Clone Methods. */
  //@{
  /**
   * Make a simple clone of this object.
   * @return a pointer to the new object.
   */
  inline virtual IBPtr clone() const;

  /** Make a clone of this object, possibly modifying the cloned object
   * to make it sane.
   * @return a pointer to the new object.
   */
  inline virtual IBPtr fullclone() const;
  //@}


// If needed, insert declarations of virtual function defined in the
// InterfacedBase class here (using ThePEG-interfaced-decl in Emacs).

protected:

  /**
   * If testing mode is active. /CF
   */
  bool thestMode;

  /**
   * Exception indicating inconsistent colour lines.
   */
  struct ColourIndexException: public Exception {};

  /**
   * Exception swinging decayed dipole.
   */
  struct SwingConsistencyException: public Exception {};

private:

  /**
   * Controls how fast the swings are. default is 1.
   */
  double theLambda;

  /**
   * The assignment operator is private and must never be called.
   * In fact, it should not even be implemented.
   */
  Swinger & operator=(const Swinger &);

};

}

#include "Swinger.icc"

#endif /* DIPSY_Swinger_H */