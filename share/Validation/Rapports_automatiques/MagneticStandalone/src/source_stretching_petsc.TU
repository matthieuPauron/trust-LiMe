****************************************************************************************************************************************************************
***                                WIDEN YOUR WINDOW TO 160 CHARACTERS.  Use 'enscript -r -fCourier9' to print this document                                 ***
****************************************************************************************************************************************************************

------------------------------------------------------------------ PETSc Performance Summary: ------------------------------------------------------------------

/home/matthieu/Development/trust-LiMe/trust-lime_opt on a  named matthieu-precisiont5610 with 1 process, by matthieu on Sun Oct 11 22:51:02 2026
Using PETSc Release Version 3.23.7, Sep 29, 2025

                         Max       Max/Min     Avg       Total
Time (sec):           8.618e-03     1.000   8.618e-03
Objects:              0.000e+00     0.000   0.000e+00
Flops:                1.594e+04     1.000   1.594e+04  1.594e+04
Flops/sec:            1.850e+06     1.000   1.850e+06  1.850e+06
MPI Msg Count:        0.000e+00     0.000   0.000e+00  0.000e+00
MPI Msg Len (bytes):  0.000e+00     0.000   0.000e+00  0.000e+00
MPI Reductions:       0.000e+00     0.000

Flop counting convention: 1 flop = 1 real number operation of type (multiply/divide/add/subtract)
                            e.g., VecAXPY() for real vectors of length N --> 2N flops
                            and VecAXPY() for complex vectors of length N --> 8N flops

Summary of Stages:   ----- Time ------  ----- Flop ------  --- Messages ---  -- Message Lengths --  -- Reductions --
                        Avg     %Total     Avg     %Total    Count   %Total     Avg         %Total    Count   %Total
 0:      Main Stage: 1.6209e-03  18.8%  2.5960e+03  16.3%  0.000e+00   0.0%  0.000e+00        0.0%  0.000e+00   0.0%
 2:        KSPSolve: 3.7788e-04   4.4%  1.3346e+04  83.7%  0.000e+00   0.0%  0.000e+00        0.0%  0.000e+00   0.0%

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

MatLUFactorNum         1 1.0 6.4920e-06 1.0 2.60e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 16  0  0  0   0 100  0  0  0   400
MatILUFactorSym        1 1.0 1.5059e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   1  0  0  0  0     0
MatAssemblyBegin       2 1.0 4.7000e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatAssemblyEnd         2 1.0 4.1280e-06 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatGetRowIJ            1 1.0 2.3100e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
MatGetOrdering         1 1.0 1.6241e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   1  0  0  0  0     0
KSPSetUp               1 1.0 2.2472e-05 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   1  0  0  0  0     0
PCSetUp                1 1.0 6.6556e-05 1.0 2.60e+03 1.0 0.0e+00 0.0e+00 0.0e+00  1 16  0  0  0   4 100  0  0  0    39
VecAssemblyBegin       4 1.0 2.8000e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecAssemblyEnd         4 1.0 2.1000e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0

--- Event Stage 2: KSPSolve

MatMult                2 1.0 2.2950e-06 1.0 4.35e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 27  0  0  0   1 33  0  0  0  1896
MatSolve               2 1.0 3.4470e-06 1.0 4.35e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 27  0  0  0   1 33  0  0  0  1263
MatLUFactorNum         1 1.0 5.1200e-06 1.0 2.60e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 16  0  0  0   1 19  0  0  0   507
KSPSolve               2 1.0 3.5871e-04 1.0 1.33e+04 1.0 0.0e+00 0.0e+00 0.0e+00  4 84  0  0  0  95 100  0  0  0    37
PCSetUp                1 1.0 6.4930e-06 1.0 2.60e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 16  0  0  0   2 19  0  0  0   400
PCApply                2 1.0 3.9370e-06 1.0 4.35e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0 27  0  0  0   1 33  0  0  0  1105
VecNorm                2 1.0 4.4100e-07 1.0 1.02e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0  6  0  0  0   0  8  0  0  0  2317
VecCopy                2 1.0 6.1100e-07 1.0 0.00e+00 0.0 0.0e+00 0.0e+00 0.0e+00  0  0  0  0  0   0  0  0  0  0     0
VecAXPY                2 1.0 6.5100e-07 1.0 1.02e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0  6  0  0  0   0  8  0  0  0  1573
VecNormalize           2 1.0 1.0510e-06 1.0 1.02e+03 1.0 0.0e+00 0.0e+00 0.0e+00  0  6  0  0  0   0  8  0  0  0   972
------------------------------------------------------------------------------------------------------------------------

Object Type          Creations   Destructions. Reports information only for process 0.

--- Event Stage 0: Main Stage

       Krylov Solver     0              1
              Matrix     3              3
      Preconditioner     0              1
              Vector     8              8
           Index Set     3              3
    Distributed Mesh     0              1
   Star Forest Graph     0              2
     Discrete System     0              1
           Weak Form     0              1

--- Event Stage 1: CreateStage


--- Event Stage 2: KSPSolve

    Distributed Mesh     1              0
   Star Forest Graph     2              0
     Discrete System     1              0
           Weak Form     1              0
========================================================================================================================
Average time to get PetscTime(): 2.5e-08
#PETSc Option Table entries:
-log_view :/home/matthieu/Development/trust-LiMe/share/Validation/Rapports_automatiques/MagneticStandalone/src/source_stretching_petsc.TU # (source: code)
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

