// ---------------------------------------------------------------------
//
// Copyright (C) 2006 - 2020 by the deal.II authors
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


#ifndef dealii_mesh_worker_local_results_h
#define dealii_mesh_worker_local_results_h

#include <deal.II/base/config.h>

#include <deal.II/base/geometry_info.h>

#include <deal.II/lac/block_vector.h>
#include <deal.II/lac/matrix_block.h>

#include <deal.II/meshworker/vector_selector.h>

#include <functional>

DEAL_II_NAMESPACE_OPEN

// Forward declaration
#ifndef DOXYGEN
class BlockIndices;
#endif

/**
 * A collection of functions and classes for the mesh loops that are an
 * ubiquitous part of each finite element program.
 *
 * The workhorse of this namespace is the loop() function, which implements a
 * completely generic loop over all mesh cells. Since the calls to loop() are
 * error-prone due to its generality, for many applications it is advisable to
 * derive a class from MeshWorker::LocalIntegrator and use the less general
 * integration_loop() instead.
 *
 * The loop() depends on certain objects handed to it as arguments. These
 * objects are of two types, @p info objects like DoFInfo and IntegrationInfo and
 * worker objects like LocalWorker and IntegrationWorker.
 *
 * Worker objects usually do two different jobs: first, they compute the local
 * contribution of a cell or face to the global operation. Second, they
 * assemble this local contribution into the global result, whether a
 * functional, a form or a bilinear form. While the first job is particular to
 * the problem being solved, the second is generic and only depends on the
 * data structures. Therefore, base classes for workers assembling into global
 * data are provided in the namespace Assembler.
 *
 * <h3>Template argument types</h3>
 *
 * The functions loop() and cell_action() take some arguments which are
 * template parameters. Let us list the minimum requirements for these classes
 * here and describe their properties.
 *
 * <h4>ITERATOR</h4>
 *
 * Any object that has an <tt>operator++()</tt> and points to a
 * TriaAccessor or derived class.
 *
 * <h4>DOFINFO</h4>
 *
 * For an example implementation, refer to the class template DoFInfo. In
 * order to work with cell_action() and loop(), DOFINFO needs to follow the
 * following interface.
 * @code
 * class DOFINFO
 * {
 *   private:
 *     DOFINFO();
 *     DOFINFO(const DOFINFO&);
 *     DOFINFO& operator=(const DOFINFO&);
 *
 *   public:
 *     template <class CellIt>
 *     void reinit(const CellIt& c);
 *
 *     template <class CellIt, class FaceIt>
 *     void reinit(const CellIt& c, const FaceIt& f, const unsigned int n);
 *
 *     template <class CellIt, class FaceIt>
 *     void reinit(const CellIt& c, const FaceIt& f, const unsigned int n,
 *                 const unsigned int s);
 *
 *   friend template class DoFInfoBox<int dim, DOFINFO>;
 * };
 * @endcode
 *
 * The three private functions are called by DoFInfoBox and should not be
 * needed elsewhere. Obviously, they can be made public and then the friend
 * declaration at the end may be missing.
 *
 * Additionally, you will need at least one public constructor. Furthermore
 * DOFINFO is pretty useless yet: functions to interface with INTEGRATIONINFO
 * and ASSEMBLER are needed.
 *
 * DOFINFO objects are gathered in a DoFInfoBox. In those objects, we store
 * the results of local operations on each cell and its faces. Once all this
 * information has been gathered, an ASSEMBLER is used to assemble it into
 * global data.
 *
 * <h4>INFOBOX</h4>
 *
 * This type is exemplified in IntegrationInfoBox. It collects the input data
 * for actions on cells and faces in INFO objects (see below). It provides the
 * following interface to loop() and cell_action():
 *
 * @code
 * class INFOBOX
 * {
 *   public:
 *     template <int dim, class DOFINFO>
 *     void post_cell(const DoFInfoBox<dim, DOFINFO>&);
 *
 *     template <int dim, class DOFINFO>
 *     void post_faces(const DoFInfoBox<dim, DOFINFO>&);
 *
 *     INFO cell;
 *     INFO boundary;
 *     INFO face;
 *     INFO subface;
 *     INFO neighbor;
 * };
 * @endcode
 *
 * The main purpose of this class is gathering the five INFO objects, which
 * contain the temporary data used on each cell or face. The requirements on
 * these objects are listed below. Here, we only note that there need to be
 * these 5 objects with the names listed above.
 *
 * The two function templates are call back functions called in cell_action().
 * The first is called before the faces are worked on, the second after the
 * faces.
 *
 * <h4>INFO</h4>
 *
 * See IntegrationInfo for an example of these objects. They contain the
 * temporary data needed on each cell or face to compute the result. The
 * MeshWorker only uses the interface
 *
 * @code
 * class INFO
 * {
 *   public:
 *     void reinit(const DOFINFO& i);
 * };
 * @endcode
 *
 * <h3>Simplified interfaces</h3>
 *
 * Since the loop() is fairly general, a specialization integration_loop() is
 * available, which is a wrapper around loop() with a simplified interface.
 *
 * The integration_loop() function loop takes most of the information that it
 * needs to pass to loop() from an IntegrationInfoBox object. Its use is
 * explained in step-12, but in short it requires functions that do the local
 * integration on a cell, interior or boundary face, and it needs an object
 * (called "assembler") that copies these local contributions into the global
 * matrix and right hand side objects.
 *
 * Before we can run the integration loop, we have to initialize several data
 * structures in our IntegrationWorker and assembler objects. For instance, we
 * have to decide on the quadrature rule or we may need more than the default
 * update flags.
 *
 * @ingroup MeshWorker
 * @ingroup Integrators
 */
namespace MeshWorker
{
  /**
   * The class providing the scrapbook to fill with results of local
   * integration. Depending on the task the mesh worker loop is performing,
   * local results can be of different types: They can be scalars, vectors
   * of size equal to the number of degrees of freedom used in the integrals,
   * or square matrices of that same size. All of these have in common that they
   * are the result of local integration over a cell or face. Which kind of
   * object is the result of an operation is determined by the Assembler using
   * them. It is also the assembler that determines <i>how many</i> of each
   * kind of object are produced (for example, an assembler may create
   * both the local contributions to a mass and a stiffness matrix), and for
   * setting the arrays of local results to the sizes needed.
   *
   * The interface of this class allows accessing all of this information
   * via the following functions:
   *
   * <ol>
   * <li> Scalars: n_values() returns the number of scalars stored by
   * an object of this class, and they are accessed via the value() function.
   *
   * <li> Vectors: n_vectors() returns the number of vectors stored by
   * an object of this class (each vector has length equal to the number of
   * degrees of freedom on this cell on which the integration happens).
   * The vectors are accessed by the vector() function.
   *
   * <li> Matrices: n_matrices() returns the number of matrices stored,
   * each of which is a square matrix of dimension equal to the number of
   * degrees of freedom per cell. The matrices are
   * accessed by matrix() with second argument <tt>false</tt>. These are
   * matrices coupling degrees of freedom in
   * the same cell. For fluxes across faces, there is an additional set of
   * matrices of the same size, with the dimension of these matrices being
   * according to the degrees of freedom on both cells. These are accessed
   * with matrix(), using the second argument <tt>true</tt>.
   * </ol>
   *
   * The local matrices are initialized by reinit() of the @p info object and then
   * assembled into the global system by Assembler classes.
   *
   * @ingroup MeshWorker
   */
  template <typename number>
  class LocalResults
  {
  public:
    /**
     * The number of scalar values stored by the current object.
     *
     * This number is set to a nonzero value by Assembler::CellsAndFaces
     */
    unsigned int
    n_values() const;

    /**
     * The number of vectors stored by the current object.
     *
     * This number is set to a nonzero value by Assembler::ResidualSimple and
     * Assembler::ResidualLocalBlocksToGlobalBlocks.
     */
    unsigned int
    n_vectors() const;

    /**
     * The number of matrices stored by the current object.
     */
    unsigned int
    n_matrices() const;

    /**
     * The number of quadrature points in quadrature_values().
     */
    unsigned int
    n_quadrature_points() const;

    /**
     * The number of values in each quadrature point in quadrature_values().
     */
    unsigned int
    n_quadrature_values() const;

    /**
     * Read-write access to the `i`th scalar stored by this class.
     */
    number &
    value(const unsigned int i);

    /**
     * Read access to the `i`th scalar stored by this class.
     */
    number
    value(const unsigned int i) const;

    /**
     * Read-write access to the `i`th vector stored by this class
     */
    BlockVector<number> &
    vector(const unsigned int i);

    /**
     * Read-write access to the `i`th vector stored by this class
     */
    const BlockVector<number> &
    vector(const unsigned int i) const;

    /**
     * Read-write access to the `i`th matrix stored by this class.
     *
     * For an explanation of the second argument, see the documentation
     * of the current class itself.
     */
    MatrixBlock<FullMatrix<number>> &
    matrix(const unsigned int i, const bool external = false);

    /**
     * Read access to the `i`th matrix stored by this class.
     *
     * For an explanation of the second argument, see the documentation
     * of the current class itself.
     */
    const MatrixBlock<FullMatrix<number>> &
    matrix(const unsigned int i, const bool external = false) const;

    /**
     * Access to the vector #quadrature_data of data in quadrature points,
     * organized such that there is a vector for each point, containing one
     * entry for each component.
     */
    Table<2, number> &
    quadrature_values();

    /**
     * Access the <i>i</i>th value at quadrature point <i>k</i>
     */
    number &
    quadrature_value(const unsigned int k, const unsigned int i);

    /**
     * Read the <i>i</i>th value at quadrature point <i>k</i>
     */
    number
    quadrature_value(const unsigned int k, const unsigned int i) const;

    /**
     * Initialize the vector with scalar values.
     *
     * @note This function is usually only called by the assembler.
     */
    void
    initialize_numbers(const unsigned int n);

    /**
     * Initialize the vector with vector values.
     *
     * @note This function is usually only called by the assembler.
     */
    void
    initialize_vectors(const unsigned int n);

    /**
     * Allocate @p n local matrices. Additionally, set their block row and
     * column coordinates to zero. The matrices themselves are resized by
     * reinit().
     *
     * @note This function is usually only called by the assembler.
     */
    void
    initialize_matrices(const unsigned int n, bool both);

    /**
     * Allocate a local matrix for each of the global ones in @p matrices.
     * Additionally, set their block row and column coordinates. The matrices
     * themselves are resized by reinit().
     *
     * @note This function is usually only called by the assembler.
     */
    template <typename MatrixType>
    void
    initialize_matrices(const MatrixBlockVector<MatrixType> &matrices,
                        bool                                 both);

    /**
     * Allocate a local matrix for each of the global level objects in @p
     * matrices. Additionally, set their block row and column coordinates. The
     * matrices themselves are resized by reinit().
     *
     * @note This function is usually only called by the assembler.
     */
    template <typename MatrixType>
    void
    initialize_matrices(const MGMatrixBlockVector<MatrixType> &matrices,
                        bool                                   both);

    /**
     * Initialize quadrature values to <tt>nv</tt> values in <tt>np</tt>
     * quadrature points.
     */
    void
    initialize_quadrature(const unsigned int np, const unsigned int nv);

    /**
     * Reinitialize matrices for new cell. Does not resize any of the data
     * vectors stored in this object, but resizes the vectors in #R and the
     * matrices in #M1 and #M2 for hp and sets them to zero.
     */
    void
    reinit(const BlockIndices &local_sizes);

    template <class StreamType>
    void
    print_debug(StreamType &os) const;

    /**
     * The memory used by this object.
     */
    std::size_t
    memory_consumption() const;

  private:
    /**
     * The local numbers, computed on a cell or on a face.
     */
    std::vector<number> J;

    /**
     * The local vectors. This field is public, so that local integrators can
     * write to it.
     */
    std::vector<BlockVector<number>> R;

    /**
     * The local matrices coupling degrees of freedom in the cell itself or
     * within the first cell on a face.
     */
    std::vector<MatrixBlock<FullMatrix<number>>> M1;

    /**
     * The local matrices coupling test functions on the cell with trial
     * functions on the other cell.
     *
     * Only used on interior faces.
     */
    std::vector<MatrixBlock<FullMatrix<number>>> M2;

    /**
     * Values in quadrature points for writing into patch data.
     */
    Table<2, number> quadrature_data;
  };

  //----------------------------------------------------------------------//

  template <typename number>
  inline void
  LocalResults<number>::initialize_numbers(const unsigned int n)
  {
    J.resize(n);
  }


  template <typename number>
  inline void
  LocalResults<number>::initialize_vectors(const unsigned int n)
  {
    R.resize(n);
  }


  template <typename number>
  template <typename MatrixType>
  inline void
  LocalResults<number>::initialize_matrices(
    const MatrixBlockVector<MatrixType> &matrices,
    bool                                 both)
  {
    M1.resize(matrices.size());
    if (both)
      M2.resize(matrices.size());
    for (unsigned int i = 0; i < matrices.size(); ++i)
      {
        const unsigned int row = matrices.block(i).row;
        const unsigned int col = matrices.block(i).column;

        M1[i].row    = row;
        M1[i].column = col;
        if (both)
          {
            M2[i].row    = row;
            M2[i].column = col;
          }
      }
  }


  template <typename number>
  template <typename MatrixType>
  inline void
  LocalResults<number>::initialize_matrices(
    const MGMatrixBlockVector<MatrixType> &matrices,
    bool                                   both)
  {
    M1.resize(matrices.size());
    if (both)
      M2.resize(matrices.size());
    for (unsigned int i = 0; i < matrices.size(); ++i)
      {
        const MGLevelObject<MatrixBlock<MatrixType>> &o = matrices.block(i);
        const unsigned int row                          = o[o.min_level()].row;
        const unsigned int col = o[o.min_level()].column;

        M1[i].row    = row;
        M1[i].column = col;
        if (both)
          {
            M2[i].row    = row;
            M2[i].column = col;
          }
      }
  }


  template <typename number>
  inline void
  LocalResults<number>::initialize_matrices(const unsigned int n,
                                            const bool         both)
  {
    M1.resize(n);
    if (both)
      M2.resize(n);
    for (unsigned int i = 0; i < n; ++i)
      {
        M1[i].row    = 0;
        M1[i].column = 0;
        if (both)
          {
            M2[i].row    = 0;
            M2[i].column = 0;
          }
      }
  }


  template <typename number>
  inline void
  LocalResults<number>::initialize_quadrature(const unsigned int np,
                                              const unsigned int nv)
  {
    quadrature_data.reinit(np, nv);
  }


  template <typename number>
  inline unsigned int
  LocalResults<number>::n_values() const
  {
    return J.size();
  }


  template <typename number>
  inline unsigned int
  LocalResults<number>::n_vectors() const
  {
    return R.size();
  }


  template <typename number>
  inline unsigned int
  LocalResults<number>::n_matrices() const
  {
    return M1.size();
  }


  template <typename number>
  inline unsigned int
  LocalResults<number>::n_quadrature_points() const
  {
    return quadrature_data.n_rows();
  }


  template <typename number>
  inline unsigned int
  LocalResults<number>::n_quadrature_values() const
  {
    return quadrature_data.n_cols();
  }


  template <typename number>
  inline number &
  LocalResults<number>::value(const unsigned int i)
  {
    AssertIndexRange(i, J.size());
    return J[i];
  }


  template <typename number>
  inline BlockVector<number> &
  LocalResults<number>::vector(const unsigned int i)
  {
    AssertIndexRange(i, R.size());
    return R[i];
  }


  template <typename number>
  inline MatrixBlock<FullMatrix<number>> &
  LocalResults<number>::matrix(const unsigned int i, const bool external)
  {
    if (external)
      {
        AssertIndexRange(i, M2.size());
        return M2[i];
      }
    AssertIndexRange(i, M1.size());
    return M1[i];
  }


  template <typename number>
  inline number &
  LocalResults<number>::quadrature_value(const unsigned int k,
                                         const unsigned int i)
  {
    return quadrature_data(k, i);
  }


  template <typename number>
  inline Table<2, number> &
  LocalResults<number>::quadrature_values()
  {
    return quadrature_data;
  }


  template <typename number>
  inline number
  LocalResults<number>::value(const unsigned int i) const
  {
    AssertIndexRange(i, J.size());
    return J[i];
  }


  template <typename number>
  inline const BlockVector<number> &
  LocalResults<number>::vector(const unsigned int i) const
  {
    AssertIndexRange(i, R.size());
    return R[i];
  }


  template <typename number>
  inline const MatrixBlock<FullMatrix<number>> &
  LocalResults<number>::matrix(const unsigned int i, const bool external) const
  {
    if (external)
      {
        AssertIndexRange(i, M2.size());
        return M2[i];
      }
    AssertIndexRange(i, M1.size());
    return M1[i];
  }


  template <typename number>
  inline number
  LocalResults<number>::quadrature_value(const unsigned int k,
                                         const unsigned int i) const
  {
    return quadrature_data(k, i);
  }


  template <typename number>
  template <class StreamType>
  void
  LocalResults<number>::print_debug(StreamType &os) const
  {
    os << "J: " << J.size() << std::endl;
    os << "R: " << R.size() << std::endl;
    for (unsigned int i = 0; i < R.size(); ++i)
      {
        os << "  " << R[i].n_blocks() << " -";
        for (unsigned int j = 0; j < R[i].n_blocks(); ++j)
          os << ' ' << R[i].block(j).size();
        os << std::endl;
      }
    os << "M: " << M1.size() << " face " << M2.size() << std::endl;
    for (unsigned int i = 0; i < M1.size(); ++i)
      {
        os << "  " << M1[i].row << ',' << M1[i].column << ' '
           << M1[i].matrix.m() << 'x' << M1[i].matrix.n();
        if (i < M2.size())
          os << " face " << M2[i].row << ',' << M2[i].column << ' '
             << M2[i].matrix.m() << 'x' << M2[i].matrix.n();
        os << std::endl;
      }
  }

} // namespace MeshWorker


DEAL_II_NAMESPACE_CLOSE

#endif
