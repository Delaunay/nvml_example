#----------------------------------------------------------------------
# Libraries have been installed in:
#    /usr/local/lib/slurm
#
# If you ever happen to want to link against installed libraries
# in a given directory, LIBDIR, you must either use libtool, and
# specify the full pathname of the library, or use the '-LLIBDIR'
# flag during linking and do at least one of the following:
#    - add LIBDIR to the 'LD_LIBRARY_PATH' environment variable
#      during execution
#    - add LIBDIR to the 'LD_RUN_PATH' environment variable
#      during linking
#    - use the '-Wl,-rpath -Wl,LIBDIR' linker flag
#    - have your system administrator add LIBDIR to '/etc/ld.so.conf'
#
# See any operating system documentation about shared libraries for
# more information, such as the ld(1) and ld.so(8) manual pages.
#----------------------------------------------------------------------


SLURM_INCLUDES=../slurm
ARGUMENTS=-c -fPIC -DHAVE_CONFIG_H -Islurm -DNUMA_VERSION1_COMPATIBILITY -g -O2 -pthread -ggdb3 -Wall -fno-strict-aliasing
LIBS=-lnvidia-ml

compile:
	mkdir -p build
	gcc ${ARGUMENTS} ${LIBS} profile.c -o build/acct_gather_profile_mila_gpu.o
	
	# shared lib
	gcc -shared -o build/libacct_gather_profile_mila_gpu.so build/acct_gather_profile_mila_gpu.o 
	
	# static library
	# ar rcs acct_gather_profile_mila_gpu.a acct_gather_profile_mila_gpu.o	

example:
	mkdir -p build
	gcc nvml_example.c -lnvidia-ml -o build/nvml_stat
	./build/nvml_stat

clean:
	rm -rf build

setup:
	git clone --depth 1 https://github.com/SchedMD/slurm.git

install:
	echo 'Install'
	sudo cp build/libacct_gather_profile_mila_gpu.so /usr/local/lib/slurm/acct_gather_profile_mila_gpu.so

tests:
	gcc -Wall -o build/tests -Lbuild -lacct_gather_profile_mila_gpu tests.c
	./build/tests

