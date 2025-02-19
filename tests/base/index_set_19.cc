// ---------------------------------------------------------------------
//
// Copyright (C) 2010 - 2018 by the deal.II authors
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


// test IndexSet::fill_index_vector

#include <deal.II/base/index_set.h>

#include <stdlib.h>

#include "../tests.h"


void
test()
{
  IndexSet is1(100);

  // randomly add 90 elements to each
  // set, some of which may be
  // repetitions of previous ones
  for (unsigned int i = 0; i < 9 * is1.size() / 10; ++i)
    is1.add_index(Testing::rand() % is1.size());

  std::vector<types::global_dof_index> indices;
  is1.fill_index_vector(indices);

  deallog << "Original index set: " << std::endl;
  is1.print(deallog);

  deallog << "List of indices: " << std::endl;
  for (unsigned int i = 0; i < indices.size(); ++i)
    deallog << indices[i] << ' ';
  deallog << std::endl;

  for (unsigned int i = 0; i < indices.size(); ++i)
    Assert(is1.index_within_set(indices[i]) == i, ExcInternalError());

  deallog << "OK" << std::endl;
}



int
main()
{
  initlog();

  test();
}
