// ---------------------------------------------------------------------
//
// Copyright (C) 2019 - 2020 by the deal.II authors
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


// Create a serial triangulations with different mesh-smoothing flags
// and copy them.

#include <deal.II/base/mpi.h>

#include <deal.II/distributed/fully_distributed_tria.h>
#include <deal.II/distributed/shared_tria.h>
#include <deal.II/distributed/tria.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_description.h>

#include "./tests.h"

using namespace dealii;

template <int dim>
void
test(int                                              n_refinements,
     const typename Triangulation<dim>::MeshSmoothing flag,
     MPI_Comm                                         comm)
{
  // create serial triangulation
  Triangulation<dim> basetria(flag);
  GridGenerator::hyper_cube(basetria);
  basetria.refine_global(1);

  for (int i = 1; i < n_refinements; ++i)
    {
      for (auto &cell : basetria.active_cell_iterators())
        {
          bool refinement_flag = true;
          for (int d = 0; d < dim; ++d)
            refinement_flag &= cell->center()[d] < 0.5;

          if (refinement_flag)
            cell->set_refine_flag();
        }
      basetria.execute_coarsening_and_refinement();
    }

  // partition
  GridTools::partition_triangulation_zorder(
    Utilities::MPI::n_mpi_processes(comm), basetria);
  GridTools::partition_multigrid_levels(basetria);

  // create instance of pft
  parallel::fullydistributed::Triangulation<dim> tria_pft(comm);

  // extract relevant information form serial triangulation
  const auto construction_data =
    TriangulationDescription::Utilities::create_description_from_triangulation(
      basetria, comm);

  // actually create triangulation
  tria_pft.create_triangulation(construction_data);

  for (auto &cell : tria_pft.active_cell_iterators())
    {
      CellId id        = cell->id();
      auto   cell_base = basetria.create_cell_iterator(id);
      for (unsigned int d = 0; d < dim; ++d)
        Assert(std::abs(cell->center()[d] - cell_base->center()[d]) < 1e-9,
               ExcMessage("Cells do not match"));
    }

  deallog << "OK!" << std::endl;
}

int
main(int argc, char *argv[])
{
  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);
  MPILogInitAll                    all;

  const MPI_Comm comm = MPI_COMM_WORLD;

  std::vector<typename Triangulation<2>::MeshSmoothing> flags{
    Triangulation<2>::none,
    Triangulation<2>::limit_level_difference_at_vertices};

  for (auto flag : flags)
    {
      deallog.push("2d");
      const int n_refinements = 3;
      test<2>(n_refinements, flag, comm);
      deallog.pop();
    }
}
