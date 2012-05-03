/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
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
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#include "Albany_SolutionMaxValueResponseFunction.hpp"
#include "Petra_Converters.hpp"
#include "Epetra_Comm.h"
#include "Teuchos_CommHelpers.hpp"
#include "Tpetra_DistObject.hpp"

Albany::SolutionMaxValueResponseFunction::
SolutionMaxValueResponseFunction(const Teuchos::RCP<const Epetra_Comm>& comm,
				 int neq_, int eq_, bool interleavedOrdering_) :
  SamplingBasedScalarResponseFunction(comm),
  comm_(comm), 
  neq(neq_), eq(eq_), interleavedOrdering(interleavedOrdering_)
{
}

Albany::SolutionMaxValueResponseFunction::
~SolutionMaxValueResponseFunction()
{
}

unsigned int
Albany::SolutionMaxValueResponseFunction::
numResponses() const 
{
  return 1;
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateResponse(const double current_time,
		 const Epetra_Vector* xdot,
		 const Epetra_Vector& x,
		 const Teuchos::Array<ParamVec>& p,
		 Epetra_Vector& g)
{
  int index;
  computeMaxValue(x, g[0], index);
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateResponseT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 Tpetra_Vector& gT)
{
  int index;
  Teuchos::ArrayRCP<ST> gT_nonconstView = gT.get1dViewNonConst();
  computeMaxValueT(xT, gT_nonconstView[0], index);
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateTangent(const double alpha, 
		const double beta,
		const double current_time,
		bool sum_derivs,
		const Epetra_Vector* xdot,
		const Epetra_Vector& x,
		const Teuchos::Array<ParamVec>& p,
		ParamVec* deriv_p,
		const Epetra_MultiVector* Vxdot,
		const Epetra_MultiVector* Vx,
		const Epetra_MultiVector* Vp,
		Epetra_Vector* g,
		Epetra_MultiVector* gx,
		Epetra_MultiVector* gp)
{
  Teuchos::RCP<Epetra_MultiVector> dgdx;
  if (gx != NULL && Vx != NULL)
    dgdx = Teuchos::rcp(new Epetra_MultiVector(x.Map(), 1));
  else
    dgdx = Teuchos::rcp(gx,false);
  evaluateGradient(current_time, xdot, x, p, deriv_p, g, dgdx.get(), NULL, gp);
  if (gx != NULL && Vx != NULL)
    gx->Multiply('T', 'N', alpha, *dgdx, *Vx, 0.0);
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateTangentT(const double alpha, 
		const double beta,
		const double current_time,
		bool sum_derivs,
		const Tpetra_Vector* xdotT,
		const Tpetra_Vector& xT,
		const Teuchos::Array<ParamVec>& p,
		ParamVec* deriv_p,
		const Tpetra_MultiVector* VxdotT,
		const Tpetra_MultiVector* VxT,
		const Tpetra_MultiVector* VpT,
		Tpetra_Vector* gT,
		Tpetra_MultiVector* gxT,
		Tpetra_MultiVector* gpT)
{
  Teuchos::RCP<Tpetra_MultiVector> dgdxT;
  if (gxT != NULL && VxT != NULL)
    dgdxT = Teuchos::rcp(new Tpetra_MultiVector(xT.getMap(), 1));
  else
    dgdxT = Teuchos::rcp(gxT,false);
  evaluateGradientT(current_time, xdotT, xT, p, deriv_p, gT, dgdxT.get(), NULL, gpT);
  Teuchos::ETransp T = Teuchos::TRANS; 
  Teuchos::ETransp N = Teuchos::NO_TRANS; 
  if (gxT != NULL && VxT != NULL)
    gxT->multiply(T, N, alpha, *dgdxT, *VxT, 0.0);
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateGradient(const double current_time,
		 const Epetra_Vector* xdot,
		 const Epetra_Vector& x,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Epetra_Vector* g,
		 Epetra_MultiVector* dg_dx,
		 Epetra_MultiVector* dg_dxdot,
		 Epetra_MultiVector* dg_dp)
{
  int global_index;
  double mxv;
  computeMaxValue(x, mxv, global_index);

  // Evaluate response g
  if (g != NULL)
    (*g)[0] = mxv;

  // Evaluate dg/dx
  if (dg_dx != NULL) {
    int im = -1;
    for (int i=0; i<x.Map().NumMyElements(); i++) {
       if (x[i] == mxv) { (*dg_dx)[0][i] = 1.0; im = i; }
       else             (*dg_dx)[0][i] = 0.0;
    }

  }

  // Evaluate dg/dxdot
  if (dg_dxdot != NULL)
    dg_dxdot->PutScalar(0.0);

  // Evaluate dg/dp
  if (dg_dp != NULL)
    dg_dp->PutScalar(0.0);
}

void
Albany::SolutionMaxValueResponseFunction::
evaluateGradientT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Tpetra_Vector* gT,
		 Tpetra_MultiVector* dg_dxT,
		 Tpetra_MultiVector* dg_dxdotT,
		 Tpetra_MultiVector* dg_dpT)
{
  int global_index;
  double mxv;
  computeMaxValueT(xT, mxv, global_index);
  
  // Evaluate response g
  if (gT != NULL) {
    Teuchos::ArrayRCP<ST> gT_nonconstView = gT->get1dViewNonConst();
    gT_nonconstView[0] = mxv;
  }

  Teuchos::ArrayRCP<const ST> xT_constView = xT.get1dView();
  
  // Evaluate dg/dx
  if (dg_dxT != NULL) {
    Teuchos::ArrayRCP<ST> dg_dxT_nonconstView;
    int im = -1;
    for (int i=0; i<xT.getMap()->getNodeNumElements(); i++) {
       dg_dxT_nonconstView = dg_dxT->getDataNonConst(0); 
       if (xT_constView[i] == mxv) { dg_dxT_nonconstView[i] = 1.0; im = i; }
       else                          dg_dxT_nonconstView[i] = 0.0;
    }

  }

  // Evaluate dg/dxdot
  if (dg_dxdotT != NULL)
    dg_dxdotT->putScalar(0.0);

  // Evaluate dg/dp
  if (dg_dpT != NULL)
    dg_dpT->putScalar(0.0);

}

void
Albany::SolutionMaxValueResponseFunction::
computeMaxValue(const Epetra_Vector& x, double& global_max, int& global_index)
{
  double my_max = -Epetra_MaxDouble;
  int my_index = -1, index;
  
  // Loop over nodes to find max value for equation eq
  int num_my_nodes = x.MyLength() / neq;
  for (int node=0; node<num_my_nodes; node++) {
    if (interleavedOrdering)  index = node*neq+eq;
    else                      index = node + eq*num_my_nodes;
    if (x[index] > my_max) {
      my_max = x[index];
      my_index = index;
    }
  }

  // Check remainder (AGS: NOT SURE HOW THIS CODE GETS CALLED?)
  if (num_my_nodes*neq+eq < x.MyLength()) {
    if (interleavedOrdering)  index = num_my_nodes*neq+eq;
    else                      index = num_my_nodes + eq*num_my_nodes;
    if (x[index] > my_max) {
      my_max = x[index];
      my_index = index;
    }
  }

  // Get max value across all proc's
  x.Comm().MaxAll(&my_max, &global_max, 1);

  // Compute min of all global indices equal to max value
  if (my_max == global_max)
    my_index = x.Map().GID(my_index);
  else
    my_index = x.GlobalLength();
  x.Comm().MinAll(&my_index, &global_index, 1);
}

void
Albany::SolutionMaxValueResponseFunction::
computeMaxValueT(const Tpetra_Vector& xT, double& global_max, int& global_index)
{
  double my_max = -Epetra_MaxDouble;
  int my_index = -1, index;
  
  Teuchos::ArrayRCP<const ST> xT_constView = xT.get1dView();
  
  // Loop over nodes to find max value for equation eq
  int num_my_nodes = xT.getLocalLength() / neq;
  for (int node=0; node<num_my_nodes; node++) {
    if (interleavedOrdering)  index = node*neq+eq;
    else                      index = node + eq*num_my_nodes;
    if (xT_constView[index] > my_max) {
      my_max = xT_constView[index];
      my_index = index;
    }
  }

  // Check remainder (AGS: NOT SURE HOW THIS CODE GETS CALLED?)
  if (num_my_nodes*neq+eq < xT.getLocalLength()) {
    if (interleavedOrdering)  index = num_my_nodes*neq+eq;
    else                      index = num_my_nodes + eq*num_my_nodes;
    if (xT_constView[index] > my_max) {
      my_max = xT_constView[index];
      my_index = index;
    }
  }

  Teuchos::RCP<const Teuchos::Comm<int> > commT = xT.getMap()->getComm(); 
  // Get max value across all proc's
  Teuchos::reduceAll(*commT, Teuchos::REDUCE_MAX, my_max, Teuchos::ptr(&global_max)); 

  // Compute min of all global indices equal to max value
  if (my_max == global_max)
    my_index = xT.getMap()->getGlobalElement(my_index);
  else
    my_index = xT.getGlobalLength();
  Teuchos::reduceAll(*commT, Teuchos::REDUCE_MIN, my_index, Teuchos::ptr(&global_index)); 
}
