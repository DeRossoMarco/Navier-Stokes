#include "NavierStokes.hpp"

int main(int argc, char *argv[]) {
    Utilities::MPI::MPI_InitFinalize mpi_init(argc, argv);

    const unsigned int N               = 8;
    const unsigned int degree_velocity = 2;
    const unsigned int degree_pressure = 1;

    Stokes problem(N, degree_velocity, degree_pressure);

    problem.setup();
    problem.assemble();
    problem.solve();
    problem.output();

    return 0;
}