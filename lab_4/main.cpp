#include <iostream>
#include <cstring>
#include <cmath>
#include <mpi.h>


double get_a() {
	return 2 * (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX)) - 1;
}

double taylor_sin(double x, const double eps) {
	x = std::fmod(x, 2 * M_PI);

	int i { 1 };
	double n { x };
	double sum { 0 };
	do {
		sum += n;
		n *= -1.0 * x * x / ((2 * i) * (2 * i + 1));
		++i;
	}
	while (std::fabs(n) > eps);
	return sum;
}

int main(int argc, char **argv) {
	if(argc != 2) {
		std::cerr << "usage: program N" << std::endl;
		std::exit(1);
	}
	const int N { std::atoi(argv[1]) };
	constexpr double coeff_i { 0.0001 };
	constexpr double sin_epsilon { 0.0000001 };

	int rank  { -1 };
	int tasks { -1 };

	double start_time { 0 };
	double end_time { 0 };

	double sum { 0 }; 
	double total_sum { 0 };

	int *index_map { nullptr }; 
	int index_map_per_process[2];

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &tasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == 0) {
		index_map = new int[tasks * 2];

		index_map[0] = 0;
		index_map[tasks * 2 - 1] = N;

		int process_index { 1 };
		int index_step    { N / tasks };
		int accumulator   { index_step };

		for(int iter = 0; iter < tasks - 1; ++iter) {
			index_map[process_index] = accumulator - 1;
			index_map[process_index + 1] = accumulator;

			process_index += 2;
			accumulator += index_step;
		}
		start_time = MPI_Wtime();
	}

	MPI_Scatter(index_map, 2, MPI_INT, index_map_per_process, 2, MPI_INT, 0, MPI_COMM_WORLD);

	std::srand(rank);
	for(int i = index_map_per_process[0]; i <= index_map_per_process[1]; ++i) {
		sum += get_a() * taylor_sin(i * coeff_i, sin_epsilon);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Reduce(&sum, &total_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);
	if(rank == 0) {
		end_time = MPI_Wtime();
		std::cout << "Sum of a * sin(x) = " 
				  << total_sum << std::endl
				  << "Time used: " 
				  << end_time - start_time 
				  << std::endl << std::endl;

		// generate the same random values
		int rand_index = 0;
		auto rand_a = new double[N + 1];
		for(int task = 0; task < tasks; ++task) {
			srand(task);
			for(int i = index_map[task * 2]; i <= index_map[task * 2 + 1]; ++i) {
				rand_a[rand_index++] = get_a();
			}
		}
		delete [] index_map; 

		total_sum = 0;
		start_time = MPI_Wtime();
		for(int i = 0; i <= N; ++i) {
			total_sum += rand_a[i] * std::sin(i * coeff_i);
		}
		end_time = MPI_Wtime();
		std::cout << "Sum of a * sin(x) = " 
				  << total_sum << std::endl
				  << "Time used: " 
				  << end_time - start_time 
				  << std::endl;
	}
	MPI_Finalize();
	return 0;
}
