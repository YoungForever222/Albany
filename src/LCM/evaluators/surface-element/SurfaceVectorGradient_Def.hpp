//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <Intrepid_MiniTensor.h>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace LCM {

//**********************************************************************
  template<typename EvalT, typename Traits>
  SurfaceVectorGradient<EvalT, Traits>::
  SurfaceVectorGradient(const Teuchos::ParameterList& p,
                        const Teuchos::RCP<Albany::Layouts>& dl) :
    thickness      (p.get<double>("thickness")), 
    cubature       (p.get<Teuchos::RCP<Intrepid::Cubature<RealType> > >("Cubature")), 
    currentBasis   (p.get<std::string>("Current Basis Name"),dl->qp_tensor),
    refDualBasis   (p.get<std::string>("Reference Dual Basis Name"),dl->qp_tensor),
    refNormal      (p.get<std::string>("Reference Normal Name"),dl->qp_vector),
    jump           (p.get<std::string>("Vector Jump Name"),dl->qp_vector),
    weights        (p.get<std::string>("Weights Name"),dl->qp_scalar),
    defGrad        (p.get<std::string>("Surface Vector Gradient Name"),dl->qp_tensor),
    J              (p.get<std::string>("Surface Vector Gradient Determinant Name"),dl->qp_scalar),
    weightedAverage(false),
    alpha(0.05)
  {
    if ( p.isType<std::string>("Weighted Volume Average J Name") )
      weightedAverage = p.get<bool>("Weighted Volume Average J");
    if ( p.isType<double>("Average J Stabilization Parameter Name") )
      alpha = p.get<double>("Average J Stabilization Parameter");

    this->addDependentField(currentBasis);
    this->addDependentField(refDualBasis);
    this->addDependentField(refNormal);
    this->addDependentField(jump);    
    this->addDependentField(weights);

    this->addEvaluatedField(defGrad);
    this->addEvaluatedField(J);

    this->setName("Surface Vector Gradient"+PHX::TypeString<EvalT>::value);

    std::vector<PHX::DataLayout::size_type> dims;
    dl->node_vector->dimensions(dims);
    worksetSize = dims[0];
    numNodes = dims[1];
    numDims = dims[2];

    numQPs = cubature->getNumPoints();

    numPlaneNodes = numNodes / 2;
    numPlaneDims = numDims - 1;
  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void SurfaceVectorGradient<EvalT, Traits>::
  postRegistrationSetup(typename Traits::SetupData d,
                        PHX::FieldManager<Traits>& fm)
  {
    this->utils.setFieldData(currentBasis,fm);
    this->utils.setFieldData(refDualBasis,fm);
    this->utils.setFieldData(refNormal,fm);
    this->utils.setFieldData(jump,fm);
    this->utils.setFieldData(weights,fm);
    this->utils.setFieldData(defGrad,fm);
    this->utils.setFieldData(J,fm);
  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void SurfaceVectorGradient<EvalT, Traits>::
  evaluateFields(typename Traits::EvalData workset)
  {
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t pt=0; pt < numQPs; ++pt) {
        Intrepid::Vector<ScalarT> g_0(3, &currentBasis(cell, pt, 0, 0));
        Intrepid::Vector<ScalarT> g_1(3, &currentBasis(cell, pt, 1, 0));
        Intrepid::Vector<ScalarT> g_2(3, &currentBasis(cell, pt, 2, 0));
        Intrepid::Vector<ScalarT> G_2(3, &refNormal(cell, pt, 0));
        Intrepid::Vector<ScalarT> d(3, &jump(cell, pt, 0));
        Intrepid::Vector<ScalarT> G0(3, &refDualBasis(cell, pt, 0, 0));
        Intrepid::Vector<ScalarT> G1(3, &refDualBasis(cell, pt, 1, 0));
        Intrepid::Vector<ScalarT> G2(3, &refDualBasis(cell, pt, 2, 0));

        Intrepid::Tensor<ScalarT>
        Fpar(Intrepid::bun(g_0, G0) +
            Intrepid::bun(g_1, G1) +
            Intrepid::bun(g_2, G2));
        // for Jay: bun()
        Intrepid::Tensor<ScalarT> Fper((1 / thickness) * Intrepid::bun(d, G_2));

        Intrepid::Tensor<ScalarT> F = Fpar + Fper;

        defGrad(cell, pt, 0, 0) = F(0, 0);
        defGrad(cell, pt, 0, 1) = F(0, 1);
        defGrad(cell, pt, 0, 2) = F(0, 2);
        defGrad(cell, pt, 1, 0) = F(1, 0);
        defGrad(cell, pt, 1, 1) = F(1, 1);
        defGrad(cell, pt, 1, 2) = F(1, 2);
        defGrad(cell, pt, 2, 0) = F(2, 0);
        defGrad(cell, pt, 2, 1) = F(2, 1);
        defGrad(cell, pt, 2, 2) = F(2, 2);

        J(cell,pt) = Intrepid::det(F);
      }
    }

    if (weightedAverage)
    {
      ScalarT Jbar, wJbar, vol;
      for (std::size_t cell=0; cell < workset.numCells; ++cell)
      {
        Jbar = 0.0;
        vol = 0.0;
        for (std::size_t qp=0; qp < numQPs; ++qp)
        {
          Jbar += weights(cell,qp) * std::log( J(cell,qp) );
          vol  += weights(cell,qp);
        }
        Jbar /= vol;

        // Jbar = std::exp(Jbar);
        for (std::size_t qp=0; qp < numQPs; ++qp)
        {
          for (std::size_t i=0; i < numDims; ++i)
          {
            for (std::size_t j=0; j < numDims; ++j)
            {
              wJbar = std::exp( (1-alpha) * Jbar + alpha * std::log( J(cell,qp) ) );
              defGrad(cell,qp,i,j) *= std::pow( wJbar / J(cell,qp) ,1./3. );
            }
          }
          J(cell,qp) = wJbar;
        }
      }
    }

  }
  //**********************************************************************  
}