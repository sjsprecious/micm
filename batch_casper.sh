#!/bin/bash
#PBS -N MICM 
#PBS -A NTDD0004
#PBS -l select=1:ncpus=36:mpiprocs=36:mem=300GB:ngpus=0
####PBS -l gpu_type=v100
#PBS -l walltime=05:59:00
#PBS -q casper 
#PBS -j oe
#PBS -k eod

ulimit -s unlimited

cd $PBS_O_WORKDIR

# copy the preprocessor chemistry solver to the src folder
cp test/preprocessor_output/*F90 src/preprocessor_output/

# save a copy of "constants.F90" code for batch runs
cd src
cp constants.F90 constants.F90_bk
cd ..

# number of MPI tasks
nranks=36

# prefix of output file
prefix="cpu"

for n in 1 2 4 8 16 32 64 128 256 512 1024 2048
do
    # clean up the build folder
    if [ ! -d "./build" ]
    then
       mkdir build
    fi
    cd build
    rm -rf *
    
    ###################################################################################################
    # Set some options on Casper following Matt's suggestions: https://github.com/NCAR/micm/issues/16 #
    # Note that using CMAKE options may drop some linkages and lead to a compilation failure          #
    ###################################################################################################

    mycompiler="nvhpc"
    usempi="on"

    if  [ $mycompiler = "gnu" ]; then
        module purge
        module load ncarenv/1.3
        module load gnu/12.1.0
        module load openmpi/4.1.4
        module load netcdf/4.8.1
        module load ncarcompilers/0.5.0
        module load cmake/3.22.0
        export JSON_FORTRAN_HOME=/glade/scratch/sunjian/temp/json-fortran-gnu-8.3.0/build
        export NETCDF_HOME=/glade/u/apps/dav/opt/netcdf/4.8.1/gnu/12.1.0
    else
        module purge
        module load ncarenv/1.3
        module load nvhpc/22.2
        module load openmpi/4.1.1
        module load ncarcompilers/0.5.0
        module load cmake/3.22.0
        export JSON_FORTRAN_HOME=/glade/scratch/sunjian/temp/json-fortran-8.3.0/build
        export NETCDF_HOME=/glade/u/apps/dav/opt/netcdf/4.8.1/nvhpc/22.2
        if  [ $usempi = "on" ]; then
            export NCAR_INC_OPENMPI=$NCAR_LDFLAGS_OPENMPI
            export FC=mpif90
        fi
    fi

    # update dfactor value
    sed "s/dfactor = 1/dfactor = $n/g" ../src/constants.F90_bk >& ../src/constants.F90 

    # build a MICM test
    #cmake -D ENABLE_UTIL_ONLY=ON ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NSYS=ON ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_OPENACC=OFF ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON -D ENABLE_NSYS=ON ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON -D ENABLE_OPENACC=OFF ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON -D ENABLE_MPI=ON ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON -D ENABLE_MPI=ON -D NUM_TASKS:STRING=$nranks ..
    cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_NETCDF=ON -D ENABLE_OPENACC=OFF -D ENABLE_MPI=ON -D NUM_TASKS:STRING=$nranks ..
    #cmake -D ENABLE_UTIL_ONLY=ON -D ENABLE_OPENACC=OFF -D CMAKE_BUILD_TYPE=DEBUG ..
    time make VERBOSE=1       # VERBOSE shows whether the desired flags are applied or not

    # run a MICM test
    #export NV_ACC_NOTIFY=3
    make test

    # save the output to a desired directory
    outdir="../output"
    if [ ! -d $outdir ]
    then
       mkdir $outdir
    fi
    mv ./Testing/Temporary/LastTest.log $outdir/${prefix}_mpi${nranks}_dfactor${n}.log 
    mv ./test/performance/test_output.nc $outdir/${prefix}_mpi${nranks}_dfactor${n}_output.nc
    cd ..
done

# clear up the temporary files
rm -f ../src/constants.F90_bk
git checkout ../src/constants.F90
