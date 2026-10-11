****************************************************************************************************************************************************************
***                                WIDEN YOUR WINDOW TO 160 CHARACTERS.  Use 'enscript -r -fCourier9' to print this document                                 ***
****************************************************************************************************************************************************************

------------------------------------------------------------------ PETSc Performance Summary: ------------------------------------------------------------------

/home/matthieu/Development/trust-LiMe/trust-lime_opt on a  named matthieu-precisiont5610 with 1 process, by matthieu on Sun Oct 11 22:50:58 2026
Using PETSc Release Version 3.23.7, Sep 29, 2025

                         Max       Max/Min     Avg       Total
Time (sec):           1.443e-01     1.000   1.443e-01
Objects:              0.000e+00     0.000   0.000e+00
Flops:                5.884e+07     1.000   5.884e+07  5.884e+07
Flops/sec:            4.077e+08     1.000   4.077e+08  4.077e+08
MPI Msg Count:        0.000e+00     0.000   0.000e+00  0.000e+00
MPI Msg Len (bytes):  0.000e+00     0.000   0.000e+00  0.000e+00
MPI Reductions:       0.000e+00     0.000

Flop counting convention: 1 flop = 1 real number operation of type (multiply/divide/add/subtract)
                            e.g., VecAXPY() for real vectors of length N --> 2N flops
                            and VecAXPY() for complex vectors of length N --> 8N flops

Summary of Stages:   ----- Time ------  ----- Flop ------  --- Messages ---  -- Message Lengths --  -- Reductions --
                        Avg     %Total     Avg     %Total    Count   %Total     Avg         %Total    Count   %Total
 0:      Main Stage: 9.5156e-02  65.9%  2.4676e+04   0.0%  0.000e+00   0.0%  0.000e+00        0.0%  0.000e+00   0.0%
 2:        KSPSolve: 2.6212e-02  18.2%  5.8811e+07 100.0%  0.000e+00   0.0%  0.000e+00        0.0%  0.000e+00   0.0%

------------------------------------------------------------------------------------------------------------------------
See the 'Profiling' chapter of the users' manual for details on interpreting output.
Phase summary info:
   Count: number of times phase was executed
   Time and Flop: Max - maximum over all processors
                  Ratio - ratio of maximum to minimum over all processors
   Mess: number of messages sent
   AvgLen: average message length (bytes)
   Reduct: number of global reductions
   Global: entire computation
   Stage: stages of a computation. Set stages with PetscLogStagePush() and PetscLogStagePop().
      %T - percent time in this phase         %F - percent flop in this phase
      %M - percent messages in this phase     %L - percent message lengths in this phase
      %R - percent reductions in this phase
   Total Mflop/s: 1e-6 * (sum of flop over all processors)/(max time over all processors)
------------------------------------------------------------------------------------------------------------------------
Event                Count      Time (sec)     Flop                              --- Global ---  --- Stage ----  Total
                   Max Ratio  Max     Ratio   Max  Ratio  Mess   AvgLen  Reduct  %T %F %M %L %R  %T %F %M %L %R Mflop/s
------------------------------------------------------------------------------------------------------------------------

--- Event Stage 0: Main Stage

MatLUFactorNum         1 1.0 7.5623e-05 1.0 2.47e+04 1.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0 100  0  0  0   326
MatILUFactorSym        1 1.0 1.5965e-04 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatAssemblyBegin     100 1.0 1.8767e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatAssemblyEnd       100 1.0 5.6194e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatGetRowIJ            1 1.0 4.4100e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatGetOrdering         1 1.0 4.4043e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
KSPSetUp               1 1.0 3.1890e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
PCSetUp                1 1.0 3.2293e-04 1.0 2.47e+04 1.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0 100  0  0  0    76
VecAssemblyBegin     200 1.0 1.5358e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecAssemblyEnd       200 1.0 1.1006e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0

--- Event Stage 2: KSPSolve

MatMult              700 1.0 4.4795e-03 1.0 1.42e+07 1.0 0.0e+00 0.0e+00 0.0e+00  3 24  0  0  0  17 24  0  0  0  3180
MatSolve             700 1.0 1.1684e-02 1.0 1.42e+07 1.0 0.0e+00 0.0e+00 0.0e+00  8 24  0  0  0  45 24  0  0  0  1219
MatLUFactorNum        99 1.0 3.9011e-03 1.0 2.44e+06 1.0 0.0e+00 0.0e+00 0.0e+00  3  4  0  0  0  15  4  0  0  0   626
KSPSolve             100 1.0 2.5855e-02 1.0 5.88e+07 1.0 0.0e+00 0.0e+00 0.0e+00 18 100  0  0  0  99 100  0  0  0  2275
KSPGMRESOrthog       600 1.0 1.6688e-03 1.0 1.94e+07 1.0 0.0e+00 0.0e+00 0.0e+00  1 33  0  0  0   6 33  0  0  0 11596
PCSetUp               99 1.0 4.0381e-03 1.0 2.44e+06 1.0 0.0e+00 0.0e+00 0.0e+00  3  4  0  0  0  15  4  0  0  0   605
PCApply              700 1.0 1.1784e-02 1.0 1.42e+07 1.0 0.0e+00 0.0e+00 0.0e+00  8 24  0  0  0  45 24  0  0  0  1209
VecMDot              600 1.0 9.9479e-04 1.0 9.67e+06 1.0 0.0e+00 0.0e+00 0.0e+00  1 16  0  0  0   4 16  0  0  0  9725
VecNorm              700 1.0 2.1205e-04 1.0 3.22e+06 1.0 0.0e+00 0.0e+00 0.0e+00  0  5  0  0  0   1  5  0  0  0 15208
VecScale             700 1.0 1.7440e-04 1.0 1.61e+06 1.0 0.0e+00 0.0e+00 0.0e+00  0  3  0  0  0   1  3  0  0  0  9248
VecCopy              100 1.0 5.6367e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecSet               100 1.0 3.3567e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecAXPY              200 1.0 9.6378e-05 1.0 9.22e+05 1.0 0.0e+00 0.0e+00 0.0e+00  0  2  0  0  0   0  2  0  0  0  9562
VecMAXPY             700 1.0 5.8912e-04 1.0 1.24e+07 1.0 0.0e+00 0.0e+00 0.0e+00  0 21  0  0  0   2 21  0  0  0 21119
VecNormalize         700 1.0 5.3662e-04 1.0 4.84e+06 1.0 0.0e+00 0.0e+00 0.0e+00  0  8  0  0  0   2  8  0  0  0  9015
------------------------------------------------------------------------------------------------------------------------

Object Type          Creations   Destructions. Reports information only for process 0.

--- Event Stage 0: Main Stage

       Krylov Solver     0              1
              Matrix     3              3
      Preconditioner     0              1
              Vector     8             18
           Index Set     3              3
    Distributed Mesh     0              1
   Star Forest Graph     0              2
     Discrete System     0              1
           Weak Form     0              1

--- Event Stage 1: CreateStage


--- Event Stage 2: KSPSolve

              Vector    10              0
    Distributed Mesh     1              0
   Star Forest Graph     2              0
     Discrete System     1              0
           Weak Form     1              0
========================================================================================================================
Average time to get PetscTime(): 2.51e-08
#PETSc Option Table entries:
-log_view :/home/matthieu/Development/trust-LiMe/share/Validation/Rapports_automatiques/MagneticStandalone/src/vector_components_petsc.TU # (source: code)
-pc_type ilu # (source: code)
#End of PETSc Option Table entries
Compiled without FORTRAN kernels
Compiled with 64-bit PetscInt
Compiled with full precision matrices (default)
sizeof(short) 2 sizeof(int) 4 sizeof(long) 8 sizeof(void*) 8 sizeof(PetscScalar) 8 sizeof(PetscInt) 8
Configure options: --prefix=/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt --with-single-library --with-shared-libraries=0 --with-make-np=12 --with-debugging=0 --COPTFLAGS="  -mavx2 -O3 -fPIC " --CXXOPTFLAGS="  -mavx2 -O3 -fPIC " --FOPTFLAGS="  -O3 -std=legacy -fPIC -fallow-argument-mismatch" --with-fortran=yes --with-fortran-bindings=0 --with-cxx-dialect=c++20  --with-clean=1 --with-ssl=0 --with-gmsh=0 --with-gmsh-exec=/bin/ls --with-windows-graphics=0 --download-scalapack=../ScaLAPACK-6f56981cb0cabffd8c72c7d1016146c4b8e276dc.tar.gz --download-mumps=../MUMPS_5.5.1.tar.gz --download-metis=../METIS-5.1.0-p12.tar.gz --download-suitesparse=../SuiteSparse-7.7.0.tar.gz --download-parmetis=../ParMETIS-4.0.3-p9.tar.gz --download-ptscotch=../PTSCOTCH-7.0.10.tar.gz --download-hypre=../hypre-2.33.0.tar.gz --with-blaslapack-dir=/home/matthieu/Development/trust-code/lib/src/LIBLAPACK --with-cudac=0 --CUDAFLAGS= --with-mpi-dir=/home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich --with-64-bit-indices
-----------------------------------------
Libraries compiled on 2026-10-11 08:27:05 on matthieu-precisiont5610
Machine characteristics: Linux-7.0.0-30-generic-x86_64-with-glibc2.39
Using PETSc directory: /home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt
Using PETSc arch: 
-----------------------------------------

Using C compiler: /home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/bin/mpicc  -Wall -Wwrite-strings -Wno-unknown-pragmas -Wno-lto-type-mismatch -Wno-stringop-overflow -fstack-protector -fvisibility=hidden -mavx2 -O3 -fPIC 
Using Fortran compiler: /home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/bin/mpif90  -Wall -ffree-line-length-none -ffree-line-length-0 -Wno-lto-type-mismatch -Wno-unused-dummy-argument -O3 -std=legacy -fPIC -fallow-argument-mismatch   
-----------------------------------------

Using include paths: -I/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/include -I/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/include/suitesparse -I/home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/include
-----------------------------------------

Using C linker: /home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/bin/mpicc
Using Fortran linker: /home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/bin/mpif90
Using libraries: -Wl,-rpath,/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/lib -L/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/lib -lpetsc -Wl,-rpath,/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/lib -L/home/matthieu/Development/trust-code/lib/src/LIBPETSC/petsc/linux_opt/lib -Wl,-rpath,/home/matthieu/Development/trust-code/lib/src/LIBLAPACK -L/home/matthieu/Development/trust-code/lib/src/LIBLAPACK -Wl,-rpath,/home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/lib -L/home/matthieu/Development/trust-code/lib/src/LIBMPI/mpich/lib -Wl,-rpath,/usr/lib/gcc/x86_64-linux-gnu/13 -L/usr/lib/gcc/x86_64-linux-gnu/13 -lHYPRE -lspqr -lumfpack -lklu -lcholmod -lbtf -lccolamd -lcolamd -lcamd -lamd -lsuitesparseconfig -ldmumps -lmumps_common -lpord -lpthread -lscalapack -llapack -lblas -lptesmumps -lptscotchparmetisv3 -lptscotch -lptscotcherr -lesmumps -lscotch -lscotcherr -lparmetis -lmetis -lm -lX11 -lmpifort -lmpi -lgfortran -lm -lgfortran -lm -lgcc_s -lquadmath -lstdc++
-----------------------------------------

