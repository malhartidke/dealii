/* ---------------------------------------------------------------------
 *
 * Copyright (C) 2014 - 2021 by the deal.II authors
 *
 * This file is part of the deal.II library.
 *
 * The deal.II library is free software; you can use it, redistribute
 * it, and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * The full text of the license can be found in the file LICENSE.md at
 * the top level directory of deal.II.
 *
 * ---------------------------------------------------------------------

 *
 * Authors: Damien Lebrun-Grandie, Bruno Turcksin, 2014
 */

// @sect3{Include files}

// The first task as usual is to include the functionality of these well-known
// deal.II library files and some C++ header files.
#include <deal.II/base/discrete_time.h>
#include <deal.II/base/function.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_out.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/sparse_direct.h>

#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/data_out.h>

#include <fstream>
#include <iostream>
#include <cmath>
#include <map>

// This is the only include file that is new: It includes all the Runge-Kutta
// methods.
#include <deal.II/base/time_stepping.h>


// The next step is like in all previous tutorial programs: We put everything
// into a namespace of its own and then import the deal.II classes and functions
// into it.
namespace Step52
{
  using namespace dealii;

  // @sect3{The <code>Diffusion</code> class}

  // The next piece is the declaration of the main class. Most of the
  // functions in this class are not new and have been explained in previous
  // tutorials. The only interesting functions are
  // <code>evaluate_diffusion()</code> and
  // <code>id_minus_tau_J_inverse()</code>. <code>evaluate_diffusion()</code>
  // evaluates the diffusion equation, $M^{-1}(f(t,y))$, at a given time and a
  // given $y$. <code>id_minus_tau_J_inverse()</code> evaluates $\left(I-\tau
  // M^{-1} \frac{\partial f(t,y)}{\partial y}\right)^{-1}$ or equivalently
  // $\left(M-\tau \frac{\partial f}{\partial y}\right)^{-1} M$ at a given
  // time, for a given $\tau$ and $y$. This function is needed when an
  // implicit method is used.
  class Diffusion
  {
  public:
    Diffusion();

    void run();

  private:
    void setup_system();

    void assemble_system();

    double get_source(const double time, const Point<2> &point) const;

    Vector<double> evaluate_diffusion(const double          time,
                                      const Vector<double> &y) const;

    Vector<double> id_minus_tau_J_inverse(const double          time,
                                          const double          tau,
                                          const Vector<double> &y);

    void output_results(const double                     time,
                        const unsigned int               time_step,
                        TimeStepping::runge_kutta_method method) const;

    // The next three functions are the drivers for the explicit methods, the
    // implicit methods, and the embedded explicit methods respectively. The
    // driver function for embedded explicit methods returns the number of
    // steps executed given that it only takes the number of time steps passed
    // as an argument as a hint, but internally computed the optimal time step
    // itself.
    void explicit_method(const TimeStepping::runge_kutta_method method,
                         const unsigned int                     n_time_steps,
                         const double                           initial_time,
                         const double                           final_time);

    void implicit_method(const TimeStepping::runge_kutta_method method,
                         const unsigned int                     n_time_steps,
                         const double                           initial_time,
                         const double                           final_time);

    unsigned int
    embedded_explicit_method(const TimeStepping::runge_kutta_method method,
                             const unsigned int n_time_steps,
                             const double       initial_time,
                             const double       final_time);


    const unsigned int fe_degree;

    const double diffusion_coefficient;
    const double absorption_cross_section;

    Triangulation<2> triangulation;

    const FE_Q<2> fe;

    DoFHandler<2> dof_handler;

    AffineConstraints<double> constraint_matrix;

    SparsityPattern sparsity_pattern;

    SparseMatrix<double> system_matrix;
    SparseMatrix<double> mass_matrix;
    SparseMatrix<double> mass_minus_tau_Jacobian;

    SparseDirectUMFPACK inverse_mass_matrix;

    Vector<double> solution;
  };



  // We choose quadratic finite elements and we initialize the parameters.
  Diffusion::Diffusion()
    : fe_degree(2)
    , diffusion_coefficient(1. / 30.)
    , absorption_cross_section(1.)
    , fe(fe_degree)
    , dof_handler(triangulation)
  {}



  // @sect4{<code>Diffusion::setup_system</code>}
  // Now, we create the constraint matrix and the sparsity pattern. Then, we
  // initialize the matrices and the solution vector.
  void Diffusion::setup_system()
  {
    dof_handler.distribute_dofs(fe);

    VectorTools::interpolate_boundary_values(dof_handler,
                                             1,
                                             Functions::ZeroFunction<2>(),
                                             constraint_matrix);
    constraint_matrix.close();

    DynamicSparsityPattern dsp(dof_handler.n_dofs());
    DoFTools::make_sparsity_pattern(dof_handler, dsp, constraint_matrix);
    sparsity_pattern.copy_from(dsp);

    system_matrix.reinit(sparsity_pattern);
    mass_matrix.reinit(sparsity_pattern);
    mass_minus_tau_Jacobian.reinit(sparsity_pattern);
    solution.reinit(dof_handler.n_dofs());
  }



  // @sect4{<code>Diffusion::assemble_system</code>}
  // In this function, we compute $-\int D \nabla b_i \cdot \nabla b_j
  // d\boldsymbol{r} - \int \Sigma_a b_i b_j d\boldsymbol{r}$ and the mass
  // matrix $\int b_i b_j d\boldsymbol{r}$. The mass matrix is then
  // inverted using a direct solver; the <code>inverse_mass_matrix</code>
  // variable will then store the inverse of the mass matrix so that
  // $M^{-1}$ can be applied to a vector using the <code>vmult()</code>
  // function of that object. (Internally, UMFPACK does not really store
  // the inverse of the matrix, but its LU factors; applying the inverse
  // matrix is then equivalent to doing one forward and one backward solves
  // with these two factors, which has the same complexity as applying an
  // explicit inverse of the matrix).
  void Diffusion::assemble_system()
  {
    system_matrix = 0.;
    mass_matrix   = 0.;

    const QGauss<2> quadrature_formula(fe_degree + 1);

    FEValues<2> fe_values(fe,
                          quadrature_formula,
                          update_values | update_gradients | update_JxW_values);


    const unsigned int dofs_per_cell = fe.n_dofs_per_cell();
    const unsigned int n_q_points    = quadrature_formula.size();

    FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
    FullMatrix<double> cell_mass_matrix(dofs_per_cell, dofs_per_cell);

    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);

    for (const auto &cell : dof_handler.active_cell_iterators())
      {
        cell_matrix      = 0.;
        cell_mass_matrix = 0.;

        fe_values.reinit(cell);

        for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
          for (unsigned int i = 0; i < dofs_per_cell; ++i)
            for (unsigned int j = 0; j < dofs_per_cell; ++j)
              {
                cell_matrix(i, j) +=
                  ((-diffusion_coefficient *                // (-D
                      fe_values.shape_grad(i, q_point) *    //  * grad phi_i
                      fe_values.shape_grad(j, q_point)      //  * grad phi_j
                    - absorption_cross_section *            //  -Sigma
                        fe_values.shape_value(i, q_point) * //  * phi_i
                        fe_values.shape_value(j, q_point))  //  * phi_j)
                   * fe_values.JxW(q_point));               // * dx
                cell_mass_matrix(i, j) += fe_values.shape_value(i, q_point) *
                                          fe_values.shape_value(j, q_point) *
                                          fe_values.JxW(q_point);
              }

        cell->get_dof_indices(local_dof_indices);

        constraint_matrix.distribute_local_to_global(cell_matrix,
                                                     local_dof_indices,
                                                     system_matrix);
        constraint_matrix.distribute_local_to_global(cell_mass_matrix,
                                                     local_dof_indices,
                                                     mass_matrix);
      }

    inverse_mass_matrix.initialize(mass_matrix);
  }



  // @sect4{<code>Diffusion::get_source</code>}
  //
  // In this function, the source term of the equation for a given time and a
  // given point is computed.
  double Diffusion::get_source(const double time, const Point<2> &point) const
  {
    const double intensity = 10.;
    const double frequency = numbers::PI / 10.;
    const double b         = 5.;
    const double x         = point(0);

    return intensity *
           (frequency * std::cos(frequency * time) * (b * x - x * x) +
            std::sin(frequency * time) *
              (absorption_cross_section * (b * x - x * x) +
               2. * diffusion_coefficient));
  }



  // @sect4{<code>Diffusion::evaluate_diffusion</code>}
  //
  // Next, we evaluate the weak form of the diffusion equation at a given time
  // $t$ and for a given vector $y$. In other words, as outlined in the
  // introduction, we evaluate $M^{-1}(-{\cal D}y - {\cal A}y + {\cal
  // S})$. For this, we have to apply the matrix $-{\cal D} - {\cal A}$
  // (previously computed and stored in the variable
  // <code>system_matrix</code>) to $y$ and then add the source term which we
  // integrate as we usually do. (Integrating up the solution could be done
  // using VectorTools::create_right_hand_side() if you wanted to save a few
  // lines of code, or wanted to take advantage of doing the integration in
  // parallel.) The result is then multiplied by $M^{-1}$.
  Vector<double> Diffusion::evaluate_diffusion(const double          time,
                                               const Vector<double> &y) const
  {
    Vector<double> tmp(dof_handler.n_dofs());
    tmp = 0.;
    system_matrix.vmult(tmp, y);

    const QGauss<2> quadrature_formula(fe_degree + 1);

    FEValues<2> fe_values(fe,
                          quadrature_formula,
                          update_values | update_quadrature_points |
                            update_JxW_values);


    const unsigned int dofs_per_cell = fe.n_dofs_per_cell();
    const unsigned int n_q_points    = quadrature_formula.size();

    Vector<double> cell_source(dofs_per_cell);

    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);

    for (const auto &cell : dof_handler.active_cell_iterators())
      {
        cell_source = 0.;

        fe_values.reinit(cell);

        for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
          {
            const double source =
              get_source(time, fe_values.quadrature_point(q_point));
            for (unsigned int i = 0; i < dofs_per_cell; ++i)
              cell_source(i) += fe_values.shape_value(i, q_point) * // phi_i(x)
                                source *                            // * S(x)
                                fe_values.JxW(q_point);             // * dx
          }

        cell->get_dof_indices(local_dof_indices);

        constraint_matrix.distribute_local_to_global(cell_source,
                                                     local_dof_indices,
                                                     tmp);
      }

    Vector<double> value(dof_handler.n_dofs());
    inverse_mass_matrix.vmult(value, tmp);

    return value;
  }


  // @sect4{<code>Diffusion::id_minus_tau_J_inverse</code>}
  //
  // We compute $\left(M-\tau \frac{\partial f}{\partial y}\right)^{-1} M$. This
  // is done in several steps:
  //   - compute $M-\tau \frac{\partial f}{\partial y}$
  //   - invert the matrix to get $\left(M-\tau \frac{\partial f}
  //                                                 {\partial y}\right)^{-1}$
  //   - compute $tmp=My$
  //   - compute $z=\left(M-\tau \frac{\partial f}{\partial y}\right)^{-1} tmp =
  //   \left(M-\tau \frac{\partial f}{\partial y}\right)^{-1} My$
  //   - return z.
  Vector<double> Diffusion::id_minus_tau_J_inverse(const double /*time*/,
                                                   const double          tau,
                                                   const Vector<double> &y)
  {
    SparseDirectUMFPACK inverse_mass_minus_tau_Jacobian;

    mass_minus_tau_Jacobian.copy_from(mass_matrix);
    mass_minus_tau_Jacobian.add(-tau, system_matrix);

    inverse_mass_minus_tau_Jacobian.initialize(mass_minus_tau_Jacobian);

    Vector<double> tmp(dof_handler.n_dofs());
    mass_matrix.vmult(tmp, y);

    Vector<double> result(y);
    inverse_mass_minus_tau_Jacobian.vmult(result, tmp);

    return result;
  }



  // @sect4{<code>Diffusion::output_results</code>}
  //
  // The following function then outputs the solution in vtu files indexed by
  // the number of the time step and the name of the time stepping method. Of
  // course, the (exact) result should really be the same for all time
  // stepping method, but the output here at least allows us to compare them.
  void Diffusion::output_results(const double                     time,
                                 const unsigned int               time_step,
                                 TimeStepping::runge_kutta_method method) const
  {
    std::string method_name;

    switch (method)
      {
        case TimeStepping::FORWARD_EULER:
          {
            method_name = "forward_euler";
            break;
          }
        case TimeStepping::RK_THIRD_ORDER:
          {
            method_name = "rk3";
            break;
          }
        case TimeStepping::RK_CLASSIC_FOURTH_ORDER:
          {
            method_name = "rk4";
            break;
          }
        case TimeStepping::BACKWARD_EULER:
          {
            method_name = "backward_euler";
            break;
          }
        case TimeStepping::IMPLICIT_MIDPOINT:
          {
            method_name = "implicit_midpoint";
            break;
          }
        case TimeStepping::SDIRK_TWO_STAGES:
          {
            method_name = "sdirk";
            break;
          }
        case TimeStepping::HEUN_EULER:
          {
            method_name = "heun_euler";
            break;
          }
        case TimeStepping::BOGACKI_SHAMPINE:
          {
            method_name = "bocacki_shampine";
            break;
          }
        case TimeStepping::DOPRI:
          {
            method_name = "dopri";
            break;
          }
        case TimeStepping::FEHLBERG:
          {
            method_name = "fehlberg";
            break;
          }
        case TimeStepping::CASH_KARP:
          {
            method_name = "cash_karp";
            break;
          }
        default:
          {
            break;
          }
      }

    DataOut<2> data_out;

    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution, "solution");

    data_out.build_patches();

    data_out.set_flags(DataOutBase::VtkFlags(time, time_step));

    const std::string filename = "solution_" + method_name + "-" +
                                 Utilities::int_to_string(time_step, 3) +
                                 ".vtu";
    std::ofstream output(filename);
    data_out.write_vtu(output);

    static std::vector<std::pair<double, std::string>> times_and_names;

    static std::string method_name_prev = "";
    static std::string pvd_filename;
    if (method_name_prev != method_name)
      {
        times_and_names.clear();
        method_name_prev = method_name;
        pvd_filename     = "solution_" + method_name + ".pvd";
      }
    times_and_names.emplace_back(time, filename);
    std::ofstream pvd_output(pvd_filename);
    DataOutBase::write_pvd_record(pvd_output, times_and_names);
  }


  // @sect4{<code>Diffusion::explicit_method</code>}
  //
  // This function is the driver for all the explicit methods. At the
  // top it initializes the time stepping and the solution (by setting
  // it to zero and then ensuring that boundary value and hanging node
  // constraints are respected; of course, with the mesh we use here,
  // hanging node constraints are not in fact an issue). It then calls
  // <code>evolve_one_time_step</code> which performs one time step.
  // Time is stored and incremented through a DiscreteTime object.
  //
  // For explicit methods, <code>evolve_one_time_step</code> needs to
  // evaluate $M^{-1}(f(t,y))$, i.e, it needs
  // <code>evaluate_diffusion</code>. Because
  // <code>evaluate_diffusion</code> is a member function, it needs to
  // be bound to <code>this</code>. After each evolution step, we
  // again apply the correct boundary values and hanging node
  // constraints.
  //
  // Finally, the solution is output
  // every 10 time steps.
  void Diffusion::explicit_method(const TimeStepping::runge_kutta_method method,
                                  const unsigned int n_time_steps,
                                  const double       initial_time,
                                  const double       final_time)
  {
    const double time_step =
      (final_time - initial_time) / static_cast<double>(n_time_steps);

    solution = 0.;
    constraint_matrix.distribute(solution);

    TimeStepping::ExplicitRungeKutta<Vector<double>> explicit_runge_kutta(
      method);
    output_results(initial_time, 0, method);
    DiscreteTime time(initial_time, final_time, time_step);
    while (time.is_at_end() == false)
      {
        explicit_runge_kutta.evolve_one_time_step(
          [this](const double time, const Vector<double> &y) {
            return this->evaluate_diffusion(time, y);
          },
          time.get_current_time(),
          time.get_next_step_size(),
          solution);
        time.advance_time();

        constraint_matrix.distribute(solution);

        if (time.get_step_number() % 10 == 0)
          output_results(time.get_current_time(),
                         time.get_step_number(),
                         method);
      }
  }



  // @sect4{<code>Diffusion::implicit_method</code>}
  // This function is equivalent to <code>explicit_method</code> but for
  // implicit methods. When using implicit methods, we need to evaluate
  // $M^{-1}(f(t,y))$ and $\left(I-\tau M^{-1} \frac{\partial f(t,y)}{\partial
  // y}\right)^{-1}$ for which we use the two member functions previously
  // introduced.
  void Diffusion::implicit_method(const TimeStepping::runge_kutta_method method,
                                  const unsigned int n_time_steps,
                                  const double       initial_time,
                                  const double       final_time)
  {
    const double time_step =
      (final_time - initial_time) / static_cast<double>(n_time_steps);

    solution = 0.;
    constraint_matrix.distribute(solution);

    TimeStepping::ImplicitRungeKutta<Vector<double>> implicit_runge_kutta(
      method);
    output_results(initial_time, 0, method);
    DiscreteTime time(initial_time, final_time, time_step);
    while (time.is_at_end() == false)
      {
        implicit_runge_kutta.evolve_one_time_step(
          [this](const double time, const Vector<double> &y) {
            return this->evaluate_diffusion(time, y);
          },
          [this](const double time, const double tau, const Vector<double> &y) {
            return this->id_minus_tau_J_inverse(time, tau, y);
          },
          time.get_current_time(),
          time.get_next_step_size(),
          solution);
        time.advance_time();

        constraint_matrix.distribute(solution);

        if (time.get_step_number() % 10 == 0)
          output_results(time.get_current_time(),
                         time.get_step_number(),
                         method);
      }
  }



  // @sect4{<code>Diffusion::embedded_explicit_method</code>}
  // This function is the driver for the embedded explicit methods. It requires
  // more parameters:
  //   - coarsen_param: factor multiplying the current time step when the error
  //   is below the threshold.
  //   - refine_param: factor multiplying the current time step when the error
  //   is above the threshold.
  //   - min_delta: smallest time step acceptable.
  //   - max_delta: largest time step acceptable.
  //   - refine_tol: threshold above which the time step is refined.
  //   - coarsen_tol: threshold below which the time step is coarsen.
  //
  // Embedded methods use a guessed time step. If the error using this time step
  // is too large, the time step will be reduced. If the error is below the
  // threshold, a larger time step will be tried for the next time step.
  // <code>delta_t_guess</code> is the guessed time step produced by the
  // embedded method. In summary, time step size is potentially modified in
  // three ways:
  //   - Reducing or increasing time step size within
  //     TimeStepping::EmbeddedExplicitRungeKutta::evolve_one_time_step().
  //   - Using the calculated <code>delta_t_guess</code>.
  //   - Automatically adjusting the step size of the last time step to ensure
  //     simulation ends precisely at <code>final_time</code>. This adjustment
  //     is handled inside the DiscreteTime instance.
  unsigned int Diffusion::embedded_explicit_method(
    const TimeStepping::runge_kutta_method method,
    const unsigned int                     n_time_steps,
    const double                           initial_time,
    const double                           final_time)
  {
    const double time_step =
      (final_time - initial_time) / static_cast<double>(n_time_steps);
    const double coarsen_param = 1.2;
    const double refine_param  = 0.8;
    const double min_delta     = 1e-8;
    const double max_delta     = 10 * time_step;
    const double refine_tol    = 1e-1;
    const double coarsen_tol   = 1e-5;

    solution = 0.;
    constraint_matrix.distribute(solution);

    TimeStepping::EmbeddedExplicitRungeKutta<Vector<double>>
      embedded_explicit_runge_kutta(method,
                                    coarsen_param,
                                    refine_param,
                                    min_delta,
                                    max_delta,
                                    refine_tol,
                                    coarsen_tol);
    output_results(initial_time, 0, method);
    DiscreteTime time(initial_time, final_time, time_step);
    while (time.is_at_end() == false)
      {
        const double new_time =
          embedded_explicit_runge_kutta.evolve_one_time_step(
            [this](const double time, const Vector<double> &y) {
              return this->evaluate_diffusion(time, y);
            },
            time.get_current_time(),
            time.get_next_step_size(),
            solution);
        time.set_next_step_size(new_time - time.get_current_time());
        time.advance_time();

        constraint_matrix.distribute(solution);

        if (time.get_step_number() % 10 == 0)
          output_results(time.get_current_time(),
                         time.get_step_number(),
                         method);

        time.set_desired_next_step_size(
          embedded_explicit_runge_kutta.get_status().delta_t_guess);
      }

    return time.get_step_number();
  }



  // @sect4{<code>Diffusion::run</code>}
  //
  // The following is the main function of the program. At the top, we create
  // the grid (a $[0,5]\times [0,5]$ square) and refine it four times to get a
  // mesh that has 16 by 16 cells, for a total of 256.  We then set the boundary
  // indicator to 1 for those parts of the boundary where $x=0$ and $x=5$.
  void Diffusion::run()
  {
    GridGenerator::hyper_cube(triangulation, 0., 5.);
    triangulation.refine_global(4);

    for (const auto &cell : triangulation.active_cell_iterators())
      for (const auto &face : cell->face_iterators())
        if (face->at_boundary())
          {
            if ((face->center()[0] == 0.) || (face->center()[0] == 5.))
              face->set_boundary_id(1);
            else
              face->set_boundary_id(0);
          }

    // Next, we set up the linear systems and fill them with content so that
    // they can be used throughout the time stepping process:
    setup_system();

    assemble_system();

    // Finally, we solve the diffusion problem using several of the
    // Runge-Kutta methods implemented in namespace TimeStepping, each time
    // outputting the error at the end time. (As explained in the
    // introduction, since the exact solution is zero at the final time, the
    // error equals the numerical solution and can be computed by just taking
    // the $l_2$ norm of the solution vector.)
    unsigned int       n_steps      = 0;
    const unsigned int n_time_steps = 200;
    const double       initial_time = 0.;
    const double       final_time   = 10.;

    std::cout << "Explicit methods:" << std::endl;
    explicit_method(TimeStepping::FORWARD_EULER,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Forward Euler:            error=" << solution.l2_norm()
              << std::endl;

    explicit_method(TimeStepping::RK_THIRD_ORDER,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Third order Runge-Kutta:  error=" << solution.l2_norm()
              << std::endl;

    explicit_method(TimeStepping::RK_CLASSIC_FOURTH_ORDER,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Fourth order Runge-Kutta: error=" << solution.l2_norm()
              << std::endl;
    std::cout << std::endl;


    std::cout << "Implicit methods:" << std::endl;
    implicit_method(TimeStepping::BACKWARD_EULER,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Backward Euler:           error=" << solution.l2_norm()
              << std::endl;

    implicit_method(TimeStepping::IMPLICIT_MIDPOINT,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Implicit Midpoint:        error=" << solution.l2_norm()
              << std::endl;

    implicit_method(TimeStepping::CRANK_NICOLSON,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   Crank-Nicolson:           error=" << solution.l2_norm()
              << std::endl;

    implicit_method(TimeStepping::SDIRK_TWO_STAGES,
                    n_time_steps,
                    initial_time,
                    final_time);
    std::cout << "   SDIRK:                    error=" << solution.l2_norm()
              << std::endl;
    std::cout << std::endl;


    std::cout << "Embedded explicit methods:" << std::endl;
    n_steps = embedded_explicit_method(TimeStepping::HEUN_EULER,
                                       n_time_steps,
                                       initial_time,
                                       final_time);
    std::cout << "   Heun-Euler:               error=" << solution.l2_norm()
              << std::endl;
    std::cout << "                   steps performed=" << n_steps << std::endl;

    n_steps = embedded_explicit_method(TimeStepping::BOGACKI_SHAMPINE,
                                       n_time_steps,
                                       initial_time,
                                       final_time);
    std::cout << "   Bogacki-Shampine:         error=" << solution.l2_norm()
              << std::endl;
    std::cout << "                   steps performed=" << n_steps << std::endl;

    n_steps = embedded_explicit_method(TimeStepping::DOPRI,
                                       n_time_steps,
                                       initial_time,
                                       final_time);
    std::cout << "   Dopri:                    error=" << solution.l2_norm()
              << std::endl;
    std::cout << "                   steps performed=" << n_steps << std::endl;

    n_steps = embedded_explicit_method(TimeStepping::FEHLBERG,
                                       n_time_steps,
                                       initial_time,
                                       final_time);
    std::cout << "   Fehlberg:                 error=" << solution.l2_norm()
              << std::endl;
    std::cout << "                   steps performed=" << n_steps << std::endl;

    n_steps = embedded_explicit_method(TimeStepping::CASH_KARP,
                                       n_time_steps,
                                       initial_time,
                                       final_time);
    std::cout << "   Cash-Karp:                error=" << solution.l2_norm()
              << std::endl;
    std::cout << "                   steps performed=" << n_steps << std::endl;
  }
} // namespace Step52



// @sect3{The <code>main()</code> function}
//
// The following <code>main</code> function is similar to previous examples
// and need not be commented on.
int main()
{
  try
    {
      Step52::Diffusion diffusion;
      diffusion.run();
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl
                << exc.what() << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    };

  return 0;
}
