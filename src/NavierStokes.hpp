#ifndef NAVIERSTOKES_HPP
#define NAVIERSTOKES_HPP

#include <deal.II/base/conditional_ostream.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/distributed/fully_distributed_tria.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_renumbering.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_simplex_p.h>
#include <deal.II/fe/fe_system.h> 
#include <deal.II/fe/fe_values.h>
#include <deal.II/fe/fe_values_extractors.h>
#include <deal.II/fe/mapping_fe.h>

#include <deal.II/grid/grid_in.h>

#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/solver_gmres.h>
#include <deal.II/lac/trilinos_block_sparse_matrix.h>
#include <deal.II/lac/trilinos_parallel_block_vector.h>
#include <deal.II/lac/trilinos_precondition.h>
#include <deal.II/lac/trilinos_sparse_matrix.h>

#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/vector_tools.h>

#include <fstream>
#include <iostream>

using namespace dealii;

// Class implementing a solver for the Stokes problem.
class NavierStokes {
    public:
    static constexpr unsigned int dim = 3;

    class ForcingTerm : public Function<dim> {
        public:
        virtual void
        vector_value(const Point<dim> & /*p*/,Vector<double> &values) const override {
            values[0] = 0;
            values[1] = -g;
            values[2] = 0;
        }

        virtual double
        value(const Point<dim> & /*p*/,const unsigned int component = 0) const override {
            if (component == 1)
                return -g;
            else
                return 0.0;
        }

        protected:
        const double g = 0.0;
    };

    class InletVelocity : public Function<dim> {
        public:
        InletVelocity() : Function<dim>(dim + 1) {}

        virtual void
        vector_value(const Point<dim> & /*p*/, Vector<double> &values) const override {
            values[0] = in_velocity;

            for (unsigned int i = 1; i < dim + 1; ++i)
                values[i] = 0.0;
        }

        virtual double
        value(const Point<dim> & /*p*/, const unsigned int component = 0) const override {
            if (component == 0)
                return in_velocity;
            else
                return 0.0;
        }

        protected:
        const double in_velocity = 10.0;
    };

    class PreconditionIdentity {
        public:
        void
        vmult(TrilinosWrappers::MPI::BlockVector & dst, 
              const TrilinosWrappers::MPI::BlockVector &src) const {
            dst = src;
        }

        protected:
    };

    
    class PreconditionBlockDiagonal{
        public:
        void
        initialize(const TrilinosWrappers::SparseMatrix &velocity_stiffness_,
                   const TrilinosWrappers::SparseMatrix &pressure_mass_) {
            velocity_stiffness = &velocity_stiffness_;
            pressure_mass      = &pressure_mass_;

            preconditioner_velocity.initialize(velocity_stiffness_);
            preconditioner_pressure.initialize(pressure_mass_);
        }

        void
        vmult(TrilinosWrappers::MPI::BlockVector & dst,
              const TrilinosWrappers::MPI::BlockVector &src) const {
            SolverControl solver_control_velocity(1000, 1e-2 * src.block(0).l2_norm());
            SolverCG<TrilinosWrappers::MPI::Vector>
                solver_cg_velocity(solver_control_velocity);
            solver_cg_velocity.solve(*velocity_stiffness,
                                     dst.block(0),
                                     src.block(0),
                                     preconditioner_velocity);

            SolverControl solver_control_pressure(1000, 1e-2 * src.block(1).l2_norm());
            SolverCG<TrilinosWrappers::MPI::Vector>
                solver_cg_pressure(solver_control_pressure);
            solver_cg_pressure.solve(*pressure_mass,
                                     dst.block(1),
                                     src.block(1),
                                     preconditioner_pressure);
        }

        protected:
        const TrilinosWrappers::SparseMatrix *velocity_stiffness;

        TrilinosWrappers::PreconditionILU preconditioner_velocity;

        const TrilinosWrappers::SparseMatrix *pressure_mass;

        TrilinosWrappers::PreconditionILU preconditioner_pressure;
    };

    class PreconditionBlockTriangular {
        public:
        void
        initialize(const TrilinosWrappers::SparseMatrix &velocity_stiffness_,
                   const TrilinosWrappers::SparseMatrix &pressure_mass_,
                   const TrilinosWrappers::SparseMatrix &B_) {
            velocity_stiffness = &velocity_stiffness_;
            pressure_mass      = &pressure_mass_;
            B                  = &B_;

            preconditioner_velocity.initialize(velocity_stiffness_);
            preconditioner_pressure.initialize(pressure_mass_);
        }

        void
        vmult(TrilinosWrappers::MPI::BlockVector & dst,
              const TrilinosWrappers::MPI::BlockVector &src) const {
            SolverControl solver_control_velocity(1000, 1e-2 * src.block(0).l2_norm());
            SolverCG<TrilinosWrappers::MPI::Vector>
                solver_cg_velocity(solver_control_velocity);
            solver_cg_velocity.solve(*velocity_stiffness,
                                     dst.block(0),
                                     src.block(0),
                                     preconditioner_velocity);

            tmp.reinit(src.block(1));
            B->vmult(tmp, dst.block(0));
            tmp.sadd(-1.0, src.block(1));

            SolverControl solver_control_pressure(1000, 1e-2 * src.block(1).l2_norm());
            SolverCG<TrilinosWrappers::MPI::Vector>
                solver_cg_pressure(solver_control_pressure);
            solver_cg_pressure.solve(*pressure_mass,
                                     dst.block(1),
                                     tmp,
                                     preconditioner_pressure);
        }

        protected:
        const TrilinosWrappers::SparseMatrix *velocity_stiffness;

        TrilinosWrappers::PreconditionILU preconditioner_velocity;

        const TrilinosWrappers::SparseMatrix *pressure_mass;

        TrilinosWrappers::PreconditionILU preconditioner_pressure;

        const TrilinosWrappers::SparseMatrix *B;

        mutable TrilinosWrappers::MPI::Vector tmp;
    };

    NavierStokes(const unsigned int &N_,
                 const unsigned int &degree_velocity_,
                 const unsigned int &degree_pressure_):
        mpi_size(Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD)),
        mpi_rank(Utilities::MPI::this_mpi_process(MPI_COMM_WORLD)),
        pcout(std::cout, mpi_rank == 0),
        N(N_),
        degree_velocity(degree_velocity_),
        degree_pressure(degree_pressure_),
        mesh(MPI_COMM_WORLD)
    {}

    void
    setup();

    void
    assemble();

    void
    solve();

    void
    output();

    protected:
    // MPI parallel. //////////////////////////////////////////////////////////

    // Number of MPI processes.
    const unsigned int mpi_size;

    // This MPI process.
    const unsigned int mpi_rank;

    // Parallel output stream.
    ConditionalOStream pcout;

    // Problem definition. ////////////////////////////////////////////////////

    // Kinematic viscosity [m2/s].
    const double nu = 1;

    // Outlet pressure [Pa].
    const double p_out = 0;

    // Forcing term.
    ForcingTerm forcing_term;

    // Inlet velocity.
    InletVelocity inlet_velocity;

    // Discretization. ////////////////////////////////////////////////////////

    // Mesh refinement.
    const unsigned int N;

    // Polynomial degree used for velocity.
    const unsigned int degree_velocity;

    // Polynomial degree used for pressure.
    const unsigned int degree_pressure;

    // Mesh.
    parallel::fullydistributed::Triangulation<dim> mesh;

    // Finite element space.
    std::unique_ptr<FiniteElement<dim>> fe;

    // Quadrature formula.
    std::unique_ptr<Quadrature<dim>> quadrature;

    // Quadrature formula for face integrals.
    std::unique_ptr<Quadrature<dim - 1>> quadrature_face;

    // DoF handler.
    DoFHandler<dim> dof_handler;

    // DoFs owned by current process.
    IndexSet locally_owned_dofs;

    // DoFs owned by current process in the velocity and pressure blocks.
    std::vector<IndexSet> block_owned_dofs;

    // DoFs relevant to the current process (including ghost DoFs).
    IndexSet locally_relevant_dofs;

    // DoFs relevant to current process in the velocity and pressure blocks.
    std::vector<IndexSet> block_relevant_dofs;

    // System matrix.
    TrilinosWrappers::BlockSparseMatrix system_matrix;

    // Pressure mass matrix, needed for preconditioning. We use a block matrix for
    // convenience, but in practice we only look at the pressure-pressure block.
    TrilinosWrappers::BlockSparseMatrix pressure_mass;

    // Right-hand side vector in the linear system.
    TrilinosWrappers::MPI::BlockVector system_rhs;

    // System solution (without ghost elements).
    TrilinosWrappers::MPI::BlockVector solution_owned;

    // System solution (including ghost elements).
    TrilinosWrappers::MPI::BlockVector solution;
};

#endif
