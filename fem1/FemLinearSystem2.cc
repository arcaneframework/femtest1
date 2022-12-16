﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* FemLinearSystem2.cc                                          (C) 2022-2022 */
/*                                                                           */
/* Linear system: Matrix A + Vector x + Vector b for Ax=b.                   */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "FemLinearSystem2.h"

#include <arcane/utils/FatalErrorException.h>
#include <arcane/utils/TraceAccessor.h>
#include <arcane/utils/NumArray.h>

#include <arcane/VariableTypes.h>
#include <arcane/IItemFamily.h>
#include <arcane/ISubDomain.h>
#include <arcane/IParallelMng.h>

#include "FemUtils.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

using namespace Arcane;

extern "C++" FemLinearSystem2Impl*
createAlephFemLinearSystem2Impl(ISubDomain* sd, const VariableDoFReal& node_variable);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

class SequentialFemLinearSystem2Impl
: public TraceAccessor
, public FemLinearSystem2Impl
{
 public:

  SequentialFemLinearSystem2Impl(ISubDomain* sd, const Arcane::VariableDoFReal& dof_variable)
  : TraceAccessor(sd->traceMng())
  , m_sub_domain(sd)
  , m_dof_family(dof_variable.variable()->itemFamily())
  , m_dof_variable(dof_variable)
  {}

 public:

  void build()
  {
    Int32 nb_node = m_dof_family->allItems().size();
    m_k_matrix.resize(nb_node, nb_node);
    m_k_matrix.fill(0.0);
    m_rhs_vector.resize(nb_node);
    m_rhs_vector.fill(0.0);
  }

 private:

  void matrixAddValue(DoFLocalId row, DoFLocalId column, Real value) override
  {
    m_k_matrix(row, column) += value;
  }

  void solve() override
  {
    Int32 matrix_size = m_k_matrix.extent0();
    Arcane::MatVec::Matrix matrix(matrix_size, matrix_size);
    _convertNumArrayToCSRMatrix(matrix, m_k_matrix.span());

    Arcane::MatVec::Vector vector_b(matrix_size);
    Arcane::MatVec::Vector vector_x(matrix_size);
    {
      auto vector_b_view = vector_b.values();
      auto vector_x_view = vector_x.values();
      for (Int32 i = 0; i < matrix_size; ++i) {
        vector_b_view(i) = m_rhs_vector[i];
        vector_x_view(i) = 0.0;
      }
    }

    {
      Real epsilon = 1.0e-15;
      Arcane::MatVec::DiagonalPreconditioner p(matrix);
      Arcane::MatVec::ConjugateGradientSolver solver;
      solver.solve(matrix, vector_b, vector_x, epsilon, &p);
    }

    {
      auto vector_x_view = vector_x.values();
      ENUMERATE_ (DoF, idof, m_dof_family->allItems()) {
        DoF dof = *idof;
        m_dof_variable[dof] = vector_x_view[dof.localId()];
      }
    }
  }

  void setRHSValues(Span<const Real> values) override
  {
    Int32 index = 0;
    DoFGroup own_dofs = m_dof_family->allItems().own();
    ENUMERATE_ (DoF, idof, own_dofs) {
      DoFLocalId dof_id = *idof;
      m_rhs_vector[dof_id] = values[index];
      ++index;
    }
  }

 private:

  ISubDomain* m_sub_domain = nullptr;
  IItemFamily* m_dof_family = nullptr;
  VariableDoFReal m_dof_variable;

  NumArray<Real, MDDim2> m_k_matrix;
  //! RHS (Right Hand Side) vector
  NumArray<Real, MDDim1> m_rhs_vector;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

FemLinearSystem2::
FemLinearSystem2()
{
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

FemLinearSystem2::
~FemLinearSystem2()
{
  delete m_p;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
_checkInit()
{
  if (!m_p)
    ARCANE_FATAL("The instance is not initialized. You need to call initialize() before using this class");
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
initialize(ISubDomain* sd, const Arcane::VariableDoFReal& dof_variable)
{
  ARCANE_CHECK_POINTER(sd);
  if (m_p)
    ARCANE_FATAL("The instance is already initialized");
  IParallelMng* pm = sd->parallelMng();
  bool is_parallel = pm->isParallel();
  // If true, we use a dense debug matrix in sequential
  bool use_debug_dense_matrix = false;
  if (is_parallel || !use_debug_dense_matrix) {
    m_p = createAlephFemLinearSystem2Impl(sd, dof_variable);
  }
  else {
    auto* x = new SequentialFemLinearSystem2Impl(sd, dof_variable);
    x->build();
    m_p = x;
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
matrixAddValue(DoFLocalId row, DoFLocalId column, Real value)
{
  _checkInit();
  m_p->matrixAddValue(row, column, value);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
setRHSValues(Span<const Real> values)
{
  _checkInit();
  m_p->setRHSValues(values);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
solve()
{
  _checkInit();
  m_p->solve();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void FemLinearSystem2::
reset()
{
  delete m_p;
  m_p = nullptr;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
