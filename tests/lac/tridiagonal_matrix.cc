// ---------------------------------------------------------------------
//
// Copyright (C) 2005 - 2018 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


// Tests TridiagonalMatrix::all_zero
// Tests TridiagonalMatrix::matrix_scalar_product
// Tests TridiagonalMatrix::vmult
// Tests TridiagonalMatrix::Tvmult
// Tests TridiagonalMatrix::vmult_add
// Tests TridiagonalMatrix::Tvmult_add
// Tests TridiagonalMatrix::print


#include <deal.II/lac/tridiagonal_matrix.h>
#include <deal.II/lac/vector.h>

#include "../tests.h"



// Symmetric matrix with constant diagonals [-1,2,-1]
template <typename number>
void
matrix1(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  for (unsigned int i = 0; i < n; ++i)
    {
      M(i, i) = 2.;
      if (i > 0)
        M(i, i - 1) = -1.;
      if (i < n - 1)
        M(i, i + 1) = -1.;
    }
}


// Nonsymmetric matrix with constant diagonals [-1,2,-3]
template <typename number>
void
matrix2(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  for (unsigned int i = 0; i < n; ++i)
    {
      M(i, i) = 2.;
      if (i > 0)
        M(i, i - 1) = -1.;
      if (i < n - 1)
        M(i, i + 1) = -3.;
    }
}


// Matrix with increasing diagonals and symmetric rows
template <typename number>
void
matrix3(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  for (unsigned int i = 0; i < n; ++i)
    {
      M(i, i) = 2. + i;
      if (i > 0)
        M(i, i - 1) = -1. - i;
      if (i < n - 1)
        M(i, i + 1) = -1. - i;
    }
}


// Symmetric matrix with increasing diagonals
template <typename number>
void
matrix4(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  for (unsigned int i = 0; i < n; ++i)
    {
      M(i, i) = 2. + i;
      if (i > 0)
        M(i, i - 1) = 0. - i;
      if (i < n - 1)
        M(i, i + 1) = -1. - i;
    }
}


// Nonsymmetric matrix with increasing diagonals
template <typename number>
void
matrix5(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  for (unsigned int i = 0; i < n; ++i)
    {
      M(i, i) = 2. + i;
      if (i > 0)
        M(i, i - 1) = -1. - i;
      if (i < n - 1)
        M(i, i + 1) = 5. - 2. * i;
    }
}


template <typename number>
void
check_vmult(TridiagonalMatrix<number> &M)
{
  const unsigned int n = M.n();
  Vector<number>     u(n);
  Vector<number>     v(n);
  Vector<number>     w(n);

  for (unsigned int i = 0; i < n; ++i)
    u(i) = 1 << i;

  deallog << "u     ";
  for (unsigned int i = 0; i < u.size(); ++i)
    deallog << ' ' << std::setw(6) << u(i);
  deallog << std::endl;

  M.vmult(v, u);

  deallog << "u^TMw ";
  for (unsigned int i = 0; i < w.size(); ++i)
    {
      w    = 0.;
      w(i) = 2.;
      //      number p1 = v*w;
      number p2 = M.matrix_scalar_product(u, w);
      deallog << ' ' << std::setw(6) << p2;
    }
  deallog << std::endl;

  deallog << "vmult ";
  for (unsigned int i = 0; i < v.size(); ++i)
    deallog << ' ' << std::setw(6) << v(i);
  deallog << std::endl;

  M.Tvmult_add(v, u);
  deallog << "+Tvm  ";
  for (unsigned int i = 0; i < v.size(); ++i)
    deallog << ' ' << std::setw(6) << v(i);
  deallog << std::endl;

  M.Tvmult(v, u);
  deallog << "Tvmult";
  for (unsigned int i = 0; i < v.size(); ++i)
    deallog << ' ' << std::setw(6) << v(i);
  deallog << std::endl;

  M.vmult_add(v, u);
  deallog << "+vm   ";
  for (unsigned int i = 0; i < v.size(); ++i)
    deallog << ' ' << std::setw(6) << v(i);
  deallog << std::endl;
}


template <typename number>
void
check(unsigned int size)
{
  TridiagonalMatrix<number> M(size);
  deallog << "all_zero " << M.all_zero();
  matrix1(M);
  deallog << ' ' << M.all_zero() << std::endl;
  deallog << "Matrix [-1,2,-1]" << std::endl;
  M.print(deallog, 7, 0);
  check_vmult(M);
  matrix2(M);
  deallog << "Matrix [-1,2,-3]" << std::endl;
  M.print(deallog, 7, 0);
  check_vmult(M);
  matrix3(M);
  deallog << "Matrix increasing symmetric rows" << std::endl;
  M.print(deallog, 7, 0);
  check_vmult(M);
  matrix4(M);
  deallog << "Matrix increasing symmetric" << std::endl;
  M.print(deallog, 7, 0);
  check_vmult(M);
  matrix5(M);
  deallog << "Matrix increasing nonsymmetric" << std::endl;
  M.print(deallog, 7, 0);
  check_vmult(M);
}

int
main()
{
  std::ofstream logfile("output");
  deallog << std::fixed;
  deallog << std::setprecision(0);
  deallog.attach(logfile);

  check<double>(5);
  check<float>(4);
}
