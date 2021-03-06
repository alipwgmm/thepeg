// -*- C++ -*-
//
// SSTVertex.h is a part of ThePEG - Toolkit for HEP Event Generation
// Copyright (C) 2003-2017 Peter Richardson, Leif Lonnblad
//
// ThePEG is licenced under version 3 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
#ifndef ThePEG_SSTVertex_H
#define ThePEG_SSTVertex_H
//
// This is the declaration of the SSTVertex class.
//
#include "ThePEG/Helicity/Vertex/AbstractSSTVertex.h"
#include "ThePEG/Helicity/WaveFunction/ScalarWaveFunction.h"
#include "ThePEG/Helicity/WaveFunction/TensorWaveFunction.h"
#include "SSTVertex.fh"

namespace ThePEG {
namespace Helicity {

/** \ingroup Helicity
 *
 *  The VVTVertexclass is the implementation of the 
 *  scalar-scalar-tensor vertex. 
 *  It inherits from the AbstractSSTVertex class for the storage of the particles
 *  interacting at the vertex and implements the helicity amplitude calculations.
 *
 *  All implementations of this vertex should inherit from it and implement the
 *  virtual setCoupling member.
 *
 *  The form of the vertex is
 *  \f[
 * -\frac{i\kappa}2\left[m^2_Sg_{\mu\nu}-k_{1\mu}k_{2\nu}-k_{1\nu}k_{2\mu}
 * +g_{\mu\nu}k_1\cdot k_2\right]\epsilon^{\mu\nu}_3\phi_1\phi_2
 *  
 *  \f]
 *
 *  @see AbstractSSTVertex
 */
class SSTVertex: public AbstractSSTVertex {
  
public:

  /**
   * Standard Init function used to initialize the interfaces.
   */
  static void Init();

public:
  
  /**
   * Members to calculate the helicity amplitude expressions for vertices
   * and off-shell particles.
   */
  //@{
  /**
   * Evalulate the vertex.
   * @param q2 The scale \f$q^2\f$ for the coupling at the vertex.
   * @param sca1  The wavefunction for the first scalar.
   * @param sca2  The wavefunction for the second scalar
   * @param ten3  The wavefunction for the tensor.
   */
  Complex evaluate(Energy2 q2, const ScalarWaveFunction & sca1,
		   const ScalarWaveFunction & sca2, const TensorWaveFunction & ten3);

  /**
   * Evaluate the off-shell scalar coming from the vertex.
   * @param q2 The scale \f$q^2\f$ for the coupling at the vertex.
   * @param iopt Option of the shape of the Breit-Wigner for the off-shell scalar.
   * @param out The ParticleData pointer for the off-shell scalar.
   * @param sca1  The wavefunction for the first scalar.
   * @param ten3  The wavefunction for the tensor.
   * @param mass The mass of the off-shell particle if not taken from the ParticleData
   * object
   * @param width The width of the off-shell particle if not taken from the ParticleData
   * object
   */
  ScalarWaveFunction evaluate(Energy2 q2,int iopt,tcPDPtr out,
			      const ScalarWaveFunction & sca1,
			      const TensorWaveFunction & ten3,
			      complex<Energy> mass=-GeV, complex<Energy> width=-GeV);

  /**
   * Evaluate the off-shell tensor coming from the vertex.
   * @param q2 The scale \f$q^2\f$ for the coupling at the vertex.
   * @param iopt Option of the shape of the Breit-Wigner for the off-shell tensor.
   * @param out The ParticleData pointer for the off-shell tensor.
   * @param sca1  The wavefunction for the first scalar.
   * @param sca2  The wavefunction for the second scalar.
   * @param mass The mass of the off-shell particle if not taken from the ParticleData
   * object
   * @param width The width of the off-shell particle if not taken from the ParticleData
   * object
   */
  TensorWaveFunction evaluate(Energy2 q2,int iopt,tcPDPtr out,
			      const ScalarWaveFunction & sca1,
			      const ScalarWaveFunction & sca2,
			      complex<Energy> mass=-GeV, complex<Energy> width=-GeV);
  //@}

  /**
   *   Set coupling methods
   */
  //@{
  /**
   * Calculate the couplings for a three point interaction.
   * This method is virtual and must be implemented in 
   * classes inheriting from this.
   * @param q2 The scale \f$q^2\f$ for the coupling at the vertex.
   * @param part1 The ParticleData pointer for the first  particle.
   * @param part2 The ParticleData pointer for the second particle.
   * @param part3 The ParticleData pointer for the third  particle.
   */
  virtual void setCoupling(Energy2 q2,tcPDPtr part1,
			   tcPDPtr part2,tcPDPtr part3)=0;

  /**
   * Dummy setCouplings for a four point interaction 
   * This method is virtual and must be implemented in 
   * classes inheriting from this.
   */
  virtual void setCoupling(Energy2,tcPDPtr,tcPDPtr,tcPDPtr,tcPDPtr) {
    assert(false);
  }
  //@}
  
private:
  
  /**
   * Describe an abstract class with persistent data.
   */
  static AbstractNoPIOClassDescription<SSTVertex> initSSTVertex;
  
  /**
   * Private and non-existent assignment operator.
   */
  SSTVertex & operator=(const SSTVertex &);
  
};

}

/** @cond TRAITSPECIALIZATIONS */
  
/**
 * The following template specialization informs ThePEG about the
 * base class of SSTVertex.
 */
template <>
struct BaseClassTrait<ThePEG::Helicity::SSTVertex,1> {
  /** Typedef of the base class of SSTVertex. */
  typedef ThePEG::Helicity::AbstractSSTVertex NthBase;
};
  
/**
 * The following template specialization informs ThePEG about the
 * name of this class and the shared object where it is defined.
 */
template <>
struct ClassTraits<ThePEG::Helicity::SSTVertex>
  : public ClassTraitsBase<ThePEG::Helicity::SSTVertex> {
  
  /**
   * Return the class name.
   */
  static string className() { return "ThePEG::SSTVertex"; }
};

/** @endcond */
  
}


#endif /* ThePEG_SSTVertex_H */
