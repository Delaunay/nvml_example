#
# 
#

SLURM_INCLUDES=../slurm
ARGUMENTS=-DHAVE_CONFIG_H -Islurm -DNUMA_VERSION1_COMPATIBILITY -g -O2 -pthread -ggdb3 -Wall -fno-strict-aliasing

compile:
	mkdir -p build
	gcc -c ${ARGUMENTS} -fPIC profile.c -o build/acct_gather_profile_mila_gpu.o
	
	# shared lib
	gcc -shared -o build/libacct_gather_profile_mila_gpu.so build/acct_gather_profile_mila_gpu.o 
	
	# static library
	# ar rcs acct_gather_profile_mila_gpu.a acct_gather_profile_mila_gpu.o	

example:
	gcc nvml_example.c -lnvidia-ml -o nvml_stat && ./nvml_stat

clean:
	rm -rf build

setup:
	git clone --depth 1 https://github.com/SchedMD/slurm.git


