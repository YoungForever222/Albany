/********************************************************************\
*            Albany, Copyright (2012) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Glen Hansen, gahanse@sandia.gov                    *
\********************************************************************/


#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"

namespace HYD {

//**********************************************************************
template<typename EvalT, typename Traits>
HydrideCResid<EvalT, Traits>::
HydrideCResid(const Teuchos::ParameterList& p) :
  wBF         (p.get<std::string>                   ("Weighted BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Scalar Data Layout") ),
  wGradBF     (p.get<std::string>                   ("Weighted Gradient BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout") ),
  cGrad       (p.get<std::string>                   ("Gradient QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout") ),
  chemTerm    (p.get<std::string>                   ("Chemical Energy Term"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  stressTerm    (p.get<std::string>                 ("Stress Term"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  cResidual   (p.get<std::string>                   ("Residual Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node Scalar Data Layout") )
{

  haveNoise = p.get<bool>("Have Noise");

  this->addDependentField(wBF);
  this->addDependentField(cGrad);
  this->addDependentField(wGradBF);
  this->addDependentField(chemTerm);
  this->addDependentField(stressTerm);
  this->addEvaluatedField(cResidual);

  if(haveNoise){
    noiseTerm = PHX::MDField<ScalarT, Cell, QuadPoint> (p.get<std::string>("Langevin Noise Term"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
    this->addDependentField(noiseTerm);
  }

  gamma = p.get<double>("gamma Value");

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  worksetSize = dims[0];
  numNodes = dims[1];
  numQPs  = dims[2];
  numDims = dims[3];

  gamma_term.resize(worksetSize, numQPs, numDims);

  this->setName("HydrideCResid"+PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void HydrideCResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(wBF,fm);
  this->utils.setFieldData(cGrad,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(chemTerm,fm);
  this->utils.setFieldData(stressTerm,fm);
  if(haveNoise)
    this->utils.setFieldData(noiseTerm,fm);

  this->utils.setFieldData(cResidual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void HydrideCResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{

// Form Equation 2.2

  typedef Intrepid::FunctionSpaceTools FST;

  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
    for (std::size_t qp=0; qp < numQPs; ++qp) 
      for (std::size_t i=0; i < numDims; ++i) 

        gamma_term(cell, qp, i) = cGrad(cell,qp,i) * gamma; 

  FST::integrate<ScalarT>(cResidual, gamma_term, wGradBF, Intrepid::COMP_CPP, false); // "false" overwrites

  FST::integrate<ScalarT>(cResidual, chemTerm, wBF, Intrepid::COMP_CPP, true); // "true" sums into

  FST::integrate<ScalarT>(cResidual, stressTerm, wBF, Intrepid::COMP_CPP, true); // "true" sums into

  if(haveNoise)

    FST::integrate<ScalarT>(cResidual, noiseTerm, wBF, Intrepid::COMP_CPP, true); // "true" sums into


}

template<typename EvalT, typename Traits>
typename HydrideCResid<EvalT, Traits>::ScalarT&
HydrideCResid<EvalT, Traits>::getValue(const std::string &n) {

  if (n == "gamma") 

    return gamma;

  else 
  {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter, std::endl <<
				"Error! Logic error in getting parameter " << n <<
				" in HydrideCResid::getValue()" << std::endl);
    return gamma;
  }

}

//**********************************************************************
}

