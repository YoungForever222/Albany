//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_SolutionResponseFunction.hpp"
#if defined(ALBANY_EPETRA)
#include "Epetra_CrsMatrix.h"
#endif
#include <algorithm>

Albany::SolutionResponseFunction::
SolutionResponseFunction(
  const Teuchos::RCP<Albany::Application>& application_,
  Teuchos::ParameterList& responseParams) :
  application(application_)
{
  // Build list of DOFs we want to keep
  // This should be replaced by DOF names eventually
  int numDOF = application->getProblem()->numEquations();
  if (responseParams.isType< Teuchos::Array<int> >("Keep DOF Indices")) {
    Teuchos::Array<int> dofs =
      responseParams.get< Teuchos::Array<int> >("Keep DOF Indices");
    keepDOF = Teuchos::Array<int>(numDOF, 0);
    for (int i=0; i<dofs.size(); i++)
      keepDOF[dofs[i]] = 1;
  }
  else {
    keepDOF = Teuchos::Array<int>(numDOF, 1);
  }
}

#if defined(ALBANY_EPETRA)
void
Albany::SolutionResponseFunction::
setup()
{
  // Build culled map and importer
  Teuchos::RCP<const Epetra_Map> x_map = application->getMap();
  culled_map = buildCulledMap(*x_map, keepDOF);
  importer = Teuchos::rcp(new Epetra_Import(*culled_map, *x_map));

  // Create graph for gradient operator -- diagonal matrix
  gradient_graph =
    Teuchos::rcp(new Epetra_CrsGraph(Copy, *culled_map, 1, true));
  for (int i=0; i<culled_map->NumMyElements(); i++) {
    int row = culled_map->GID(i);
    gradient_graph->InsertGlobalIndices(row, 1, &row);
  }
  gradient_graph->FillComplete();
  gradient_graph->OptimizeStorage();

}
#endif

void
Albany::SolutionResponseFunction::
setupT()
{

  // Build culled map and importer - Tpetra
  Teuchos::RCP<const Tpetra_Map> x_mapT = application->getMapT();
  Teuchos::RCP<const Teuchos::Comm<int> > commT = application->getComm(); 
  //Tpetra version of culled_map
  culled_mapT = buildCulledMapT(*x_mapT, keepDOF);

  importerT = Teuchos::rcp(new Tpetra_Import(x_mapT, culled_mapT));

  // Create graph for gradient operator -- diagonal matrix: Tpetra version
  Teuchos::ArrayView<GO> rowAV;
  gradient_graphT =
    Teuchos::rcp(new Tpetra_CrsGraph(culled_mapT, 1));
  for (int i=0; i<culled_mapT->getNodeNumElements(); i++) {
    GO row = culled_mapT->getGlobalElement(i);
    rowAV = Teuchos::arrayView(&row, 1);
    gradient_graphT->insertGlobalIndices(row, rowAV);
  }
  gradient_graphT->fillComplete();
  //IK, 8/22/13: Tpetra_CrsGraph does not appear to have optimizeStorage() function...
  //gradient_graphT->optimizeStorage();
}

Albany::SolutionResponseFunction::
~SolutionResponseFunction()
{
}

#if defined(ALBANY_EPETRA)
Teuchos::RCP<const Epetra_Map>
Albany::SolutionResponseFunction::
responseMap() const
{
  return culled_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Albany::SolutionResponseFunction::
responseMapT() const
{
  return culled_mapT;
}

#if defined(ALBANY_EPETRA)
Teuchos::RCP<Epetra_Operator>
Albany::SolutionResponseFunction::
createGradientOp() const
{
  Teuchos::RCP<Epetra_CrsMatrix> G =
    Teuchos::rcp(new Epetra_CrsMatrix(Copy, *gradient_graph));
  G->FillComplete();
  return G;
}
#endif

Teuchos::RCP<Tpetra_Operator>
Albany::SolutionResponseFunction::
createGradientOpT() const
{
  Teuchos::RCP<Tpetra_CrsMatrix> GT =
    Teuchos::rcp(new Tpetra_CrsMatrix(gradient_graphT));
  GT->fillComplete();
  return GT;
}


void
Albany::SolutionResponseFunction::
evaluateResponseT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector* xdotdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 Tpetra_Vector& gT)
{
  cullSolutionT(xT, gT);
}


void
Albany::SolutionResponseFunction::
evaluateTangentT(const double alpha,
		const double beta,
		const double omega,
		const double current_time,
		bool sum_derivs,
		const Tpetra_Vector* xdotT,
		const Tpetra_Vector* xdotdotT,
		const Tpetra_Vector& xT,
		const Teuchos::Array<ParamVec>& p,
		ParamVec* deriv_p,
		const Tpetra_MultiVector* VxdotT,
		const Tpetra_MultiVector* VxdotdotT,
		const Tpetra_MultiVector* VxT,
		const Tpetra_MultiVector* VpT,
		Tpetra_Vector* gT,
		Tpetra_MultiVector* gxT,
		Tpetra_MultiVector* gpT)
{
  if (gT) {
    cullSolutionT(xT, *gT);
  }

  if (gxT) {
    gxT->putScalar(0.0);
    if (VxT) {
      cullSolutionT(*VxT, *gxT);
      gxT->scale(beta);
    }
  }

  if (gpT)
    gpT->putScalar(0.0);
}

#if defined(ALBANY_EPETRA)
void
Albany::SolutionResponseFunction::
evaluateGradient(const double current_time,
		 const Epetra_Vector* xdot,
		 const Epetra_Vector* xdotdot,
		 const Epetra_Vector& x,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Epetra_Vector* g,
		 Epetra_Operator* dg_dx,
		 Epetra_Operator* dg_dxdot,
		 Epetra_Operator* dg_dxdotdot,
		 Epetra_MultiVector* dg_dp)
{
  if (g)
    cullSolution(x, *g);

  if (dg_dx) {
    Epetra_CrsMatrix *dg_dx_crs = dynamic_cast<Epetra_CrsMatrix*>(dg_dx);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dx_crs == NULL);
    dg_dx_crs->PutScalar(1.0); // matrix only stores the diagonal
  }

  if (dg_dxdot) {
    Epetra_CrsMatrix *dg_dxdot_crs = dynamic_cast<Epetra_CrsMatrix*>(dg_dxdot);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dxdot_crs == NULL);
    dg_dxdot_crs->PutScalar(0.0); // matrix only stores the diagonal
  }
  if (dg_dxdotdot) {
    Epetra_CrsMatrix *dg_dxdotdot_crs = dynamic_cast<Epetra_CrsMatrix*>(dg_dxdotdot);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dxdotdot_crs == NULL);
    dg_dxdotdot_crs->PutScalar(0.0); // matrix only stores the diagonal
  }

  if (dg_dp)
    dg_dp->PutScalar(0.0);
}
#endif

void
Albany::SolutionResponseFunction::
evaluateGradientT(const double current_time,
		 const Tpetra_Vector* xdotT,
		 const Tpetra_Vector* xdotdotT,
		 const Tpetra_Vector& xT,
		 const Teuchos::Array<ParamVec>& p,
		 ParamVec* deriv_p,
		 Tpetra_Vector* gT,
		 Tpetra_Operator* dg_dxT,
		 Tpetra_Operator* dg_dxdotT,
		 Tpetra_Operator* dg_dxdotdotT,
		 Tpetra_MultiVector* dg_dpT)
{

/*
  Note: In the below, Tpetra throws an exception if one tries to "setAllToScalar()" on
  a CrsMatrix that fill is not active on. The if tests check the fill status, if fill is
  not active "resumeFill()" is called prior to "setAllToScalar()", then "fillComplete()"
  is called to put the CsrMatrix in its original state.
*/

  bool callFillComplete = false;

  if (gT)
    cullSolutionT(xT, *gT);

  if (dg_dxT) {
    Tpetra_CrsMatrix *dg_dx_crsT = dynamic_cast<Tpetra_CrsMatrix*>(dg_dxT);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dx_crsT == NULL);

    if(!dg_dx_crsT->isFillActive()){
       dg_dx_crsT->resumeFill();
       callFillComplete = true;
    }

    dg_dx_crsT->setAllToScalar(1.0); // matrix only stores the diagonal

    if(callFillComplete){
      callFillComplete = false;
      dg_dx_crsT->fillComplete();
    }
  }

  if (dg_dxdotT) {
    Tpetra_CrsMatrix *dg_dxdot_crsT = dynamic_cast<Tpetra_CrsMatrix*>(dg_dxdotT);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dxdot_crsT == NULL);

    if(!dg_dxdot_crsT->isFillActive()){
       dg_dxdot_crsT->resumeFill();
       callFillComplete = true;
    }

    dg_dxdot_crsT->setAllToScalar(0.0); // matrix only stores the diagonal

    if(callFillComplete){
      callFillComplete = false;
      dg_dxdot_crsT->fillComplete();
    }
  }

  if (dg_dxdotdotT) {
    Tpetra_CrsMatrix *dg_dxdotdot_crsT = dynamic_cast<Tpetra_CrsMatrix*>(dg_dxdotdotT);
    TEUCHOS_TEST_FOR_EXCEPT(dg_dxdotdot_crsT == NULL);

    if(!dg_dxdotdot_crsT->isFillActive()){
       dg_dxdotdot_crsT->resumeFill();
       callFillComplete = true;
    }

    dg_dxdotdot_crsT->setAllToScalar(0.0); // matrix only stores the diagonal

    if(callFillComplete){
      callFillComplete = false;
      dg_dxdotdot_crsT->fillComplete();
    }
  }

  if (dg_dpT)
    dg_dpT->putScalar(0.0);
}

void
Albany::SolutionResponseFunction::
evaluateDistParamDerivT(
      const double current_time,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& param_array,
      const std::string& dist_param_name,
      Tpetra_MultiVector* dg_dpT)
{
  if (dg_dpT)
    dg_dpT->putScalar(0.0);
}

#if defined(ALBANY_EPETRA)
Teuchos::RCP<Epetra_Map>
Albany::SolutionResponseFunction::
buildCulledMap(const Epetra_Map& x_map,
	       const Teuchos::Array<int>& keepDOF) const
{
  int numKeepDOF = std::accumulate(keepDOF.begin(), keepDOF.end(), 0);
  int Neqns = keepDOF.size();
  int N = x_map.NumMyElements(); // x_map is map for solution vector

  TEUCHOS_ASSERT( !(N % Neqns) ); // Assume that all the equations for
                                  // a given node are on the assigned
                                  // processor. I.e. need to ensure
                                  // that N is exactly Neqns-divisible

  int nnodes = N / Neqns;          // number of fem nodes
  int N_new = nnodes * numKeepDOF; // length of local x_new

  int *gids = x_map.MyGlobalElements(); // Fill local x_map into gids array
  Teuchos::Array<int> gids_new(N_new);
  int idx = 0;
  for ( int inode = 0; inode < N/Neqns ; ++inode) // For every node
    for ( int ieqn = 0; ieqn < Neqns; ++ieqn )  // Check every dof on the node
      if ( keepDOF[ieqn] == 1 )  // then want to keep this dof
	gids_new[idx++] = gids[(inode*Neqns)+ieqn];
  // end cull

  Teuchos::RCP<Epetra_Map> x_new_map =
    Teuchos::rcp( new Epetra_Map( -1, N_new, &gids_new[0], 0, x_map.Comm() ) );

  return x_new_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Albany::SolutionResponseFunction::
buildCulledMapT(const Tpetra_Map& x_mapT,
	       const Teuchos::Array<int>& keepDOF) const
{
  int numKeepDOF = std::accumulate(keepDOF.begin(), keepDOF.end(), 0);
  int Neqns = keepDOF.size();
  int N = x_mapT.getNodeNumElements(); // x_mapT is map for solution vector

  TEUCHOS_ASSERT( !(N % Neqns) ); // Assume that all the equations for
                                  // a given node are on the assigned
                                  // processor. I.e. need to ensure
                                  // that N is exactly Neqns-divisible

  int nnodes = N / Neqns;          // number of fem nodes
  int N_new = nnodes * numKeepDOF; // length of local x_new

  Teuchos::ArrayView<const GO> gidsT = x_mapT.getNodeElementList();
  Teuchos::Array<GO> gids_new(N_new);
  int idx = 0;
  for ( int inode = 0; inode < N/Neqns ; ++inode) // For every node
    for ( int ieqn = 0; ieqn < Neqns; ++ieqn )  // Check every dof on the node
      if ( keepDOF[ieqn] == 1 )  // then want to keep this dof
	gids_new[idx++] = gidsT[(inode*Neqns)+ieqn];
  // end cull

  Teuchos::RCP<const Tpetra_Map> x_new_mapT = Tpetra::createNonContigMapWithNode<LO, Tpetra_GO, KokkosNode> (gids_new, x_mapT.getComm(), x_mapT.getNode());

  return x_new_mapT;

}

#if defined(ALBANY_EPETRA)
void
Albany::SolutionResponseFunction::
cullSolution(const Epetra_MultiVector& x, Epetra_MultiVector& x_culled) const
{
  x_culled.Import(x, *importer, Insert);
}
#endif

void
Albany::SolutionResponseFunction::
cullSolutionT(const Tpetra_MultiVector& xT, Tpetra_MultiVector& x_culledT) const
{
  x_culledT.doImport(xT, *importerT, Tpetra::INSERT);
}
