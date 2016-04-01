//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ATO_STIFFNESSOBJECTIVE_HPP
#define ATO_STIFFNESSOBJECTIVE_HPP

#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Albany_ProblemUtils.hpp"
#include "ATO_TopoTools.hpp"

namespace ATO {
/**
 * \brief Description
 */
  template<typename EvalT, typename Traits>
  class StiffnessObjectiveBase :
    public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    StiffnessObjectiveBase(Teuchos::ParameterList& p,
		      const Teuchos::RCP<Albany::Layouts>& dl,
                      const Albany::MeshSpecsStruct* meshSpecs);

    void postRegistrationSetup(typename Traits::SetupData d,
			       PHX::FieldManager<Traits>& vm);

    // These functions are defined in the specializations
    void preEvaluate(typename Traits::PreEvalData d) = 0;
    void postEvaluate(typename Traits::PostEvalData d) = 0;
    void evaluateFields(typename Traits::EvalData d) = 0;

    Teuchos::RCP<const PHX::FieldTag> getEvaluatedFieldTag() const {
      return stiffness_objective_tag;
    }

    Teuchos::RCP<const PHX::FieldTag> getResponseFieldTag() const {
      return stiffness_objective_tag;
    }

  protected:

    Teuchos::Array<std::string> dFdpNames;
    std::string FName;
    std::string elementBlockName;
    static const std::string className;

    PHX::MDField<MeshScalarT,Cell,QuadPoint> qp_weights;
    PHX::MDField<RealType,Cell,Node,QuadPoint> BF;

    Teuchos::RCP< PHX::Tag<ScalarT> > stiffness_objective_tag;
    Albany::StateManager* pStateMgr;

    Teuchos::RCP<TopologyArray> topologies;

    template< typename N >
    class PenaltyModel {
      public:
        PenaltyModel( Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl );
        virtual void Evaluate(Teuchos::Array<double>& topoVals, Teuchos::RCP<TopologyArray>& topologies,
                              int cell, int qp, N& response, Teuchos::Array<N>& dResponse)=0;
        virtual void getDependentFields(Teuchos::Array<PHX::MDField<N> >& depFields)=0;
        virtual void getDependentFields(Teuchos::Array< PHX::MDField<N>* >& depFields)=0;
        void getFieldDimensions(std::vector<int>& dims);
      protected:
        int numDims, rank;
        PHX::MDField<N> gradX;
    };

    template< typename N >
    class PenaltyMixture : public PenaltyModel<N> {
      using PenaltyModel<N>::numDims;
      using PenaltyModel<N>::rank;
      using PenaltyModel<N>::gradX;
      public:
        PenaltyMixture( Teuchos::ParameterList& blockParams,
                        Teuchos::ParameterList& p,
                        const Teuchos::RCP<Albany::Layouts>& dl);
        void Evaluate(Teuchos::Array<double>& topoVals, Teuchos::RCP<TopologyArray>& topologies,
                      int cell, int qp, N& response, Teuchos::Array<N>& dResponse);
        void getDependentFields(Teuchos::Array<PHX::MDField<N> >& depFields);
        void getDependentFields(Teuchos::Array< PHX::MDField<N>* >& depFields);
      private:
        int topologyIndex;
        int functionIndex;
        Teuchos::Array<int> materialIndices;
        Teuchos::Array<int> mixtureTopologyIndices;
        Teuchos::Array<int> mixtureFunctionIndices;
        Teuchos::Array<PHX::MDField<N> > workConj;
    };

    template< typename N >
    class PenaltyMaterial : public PenaltyModel<N> {
      using PenaltyModel<N>::numDims;
      using PenaltyModel<N>::rank;
      using PenaltyModel<N>::gradX;
      public:
        PenaltyMaterial( Teuchos::ParameterList& blockParams,
                         Teuchos::ParameterList& p,
                         const Teuchos::RCP<Albany::Layouts>& dl);
        void Evaluate(Teuchos::Array<double>& topoVals, Teuchos::RCP<TopologyArray>& topologies,
                      int cell, int qp, N& response, Teuchos::Array<N>& dResponse);
        void getDependentFields(Teuchos::Array<PHX::MDField<N> >& depFields);
        void getDependentFields(Teuchos::Array< PHX::MDField<N>* >& depFields);
      private:
        int topologyIndex;
        int functionIndex;
        PHX::MDField<N> workConj;
    };

    Teuchos::RCP< PenaltyModel<ScalarT> > penaltyModel;

  };

template<typename EvalT, typename Traits>
class StiffnessObjective
   : public StiffnessObjectiveBase<EvalT, Traits> {

   using StiffnessObjectiveBase<EvalT,Traits>::penaltyModel;
   using StiffnessObjectiveBase<EvalT,Traits>::topologies;
   using StiffnessObjectiveBase<EvalT,Traits>::dFdpNames;
   using StiffnessObjectiveBase<EvalT,Traits>::FName;
   using StiffnessObjectiveBase<EvalT,Traits>::elementBlockName;
   using StiffnessObjectiveBase<EvalT,Traits>::className;
   using StiffnessObjectiveBase<EvalT,Traits>::qp_weights;
   using StiffnessObjectiveBase<EvalT,Traits>::BF;
   using StiffnessObjectiveBase<EvalT,Traits>::stiffness_objective_tag;
   using StiffnessObjectiveBase<EvalT,Traits>::pStateMgr;

public:
  StiffnessObjective(Teuchos::ParameterList& p,
              const Teuchos::RCP<Albany::Layouts>& dl,
              const Albany::MeshSpecsStruct* meshSpecs) :
  StiffnessObjectiveBase<EvalT, Traits>(p, dl, meshSpecs){}
  void preEvaluate(typename Traits::PreEvalData d){}
  void postEvaluate(typename Traits::PostEvalData d){}
  void evaluateFields(typename Traits::EvalData d){}
};

// **************************************************************
// **************************************************************
// * Specializations
// **************************************************************
// **************************************************************

// **************************************************************
// Residual
// **************************************************************
template<typename Traits>
class StiffnessObjective<PHAL::AlbanyTraits::Residual,Traits>
   : public StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits> {

   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::penaltyModel;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::topologies;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::dFdpNames;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::FName;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::elementBlockName;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::className;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::qp_weights;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::BF;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::stiffness_objective_tag;
   using StiffnessObjectiveBase<PHAL::AlbanyTraits::Residual, Traits>::pStateMgr;

public:
  StiffnessObjective(Teuchos::ParameterList& p,
              const Teuchos::RCP<Albany::Layouts>& dl,
              const Albany::MeshSpecsStruct* meshSpecs);
  void preEvaluate(typename Traits::PreEvalData d);
  void postEvaluate(typename Traits::PostEvalData d);
  void evaluateFields(typename Traits::EvalData d);
};

}

#endif
