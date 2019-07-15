FROM nvidia/cuda:10.0-cudnn7-devel-ubuntu16.04
ARG PYTHON_VERSION=3.6

#RUN apt-get update && apt-get install -y --no-install-recommends \
#         build-essential \
#         cmake \
#         git \
#         curl \
#         ca-certificates \
#         libjpeg-dev \
#         libpng-dev && \
#    rm -rf /var/lib/apt/lists/*

#RUN curl -o ~/miniconda.sh -O  https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh  && \
#    chmod +x ~/miniconda.sh && \
#    ~/miniconda.sh -b -p /opt/conda && \
#    rm ~/miniconda.sh && \
#    /opt/conda/bin/conda install -y python=$PYTHON_VERSION numpy pyyaml scipy ipython mkl mkl-include ninja cython typing && \
#    /opt/conda/bin/conda install -y -c pytorch magma-cuda100 && \
#    /opt/conda/bin/conda clean -ya

ENV PATH /opt/conda/bin:$PATH
#RUN git clone https://github.com/pytorch/vision.git && cd vision && pip install -v .
#WORKDIR /workspace
#RUN chmod -R a+w .

RUN apt-get update
RUN apt-get install -y --no-install-recommends\
	libcgroup-dev\	
        munge\
        libmunge-dev\
        libmysqlclient-dev\
        libcurl4-openssl-dev\
        liblz4-dev\
	libreadline-dev\
	libnuma-dev\
	git\
	file\
	python
        
WORKDIR /workspace

#RUN git clone --depth 1 https://github.com/SchedMD/slurm.git
RUN git clone --depth 1 https://github.com/Delaunay/slurm.git
WORKDIR /workspace/slurm

RUN ./configure && \
    make install

WORKDIR /workspace

RUN mkdir -p /etc/slurm
RUN mkdir -p /var/run/munge

ADD slurm.conf /etc/slurm/slurm.conf
ADD cgroup.conf /etc/slurm/cgroup.conf
ADD acct_gather.conf /etc/slurm/acct_gather.conf
ADD start_slurm.sh /workspace/start_slurm.sh

ENV SLURM_CONF=/etc/slurm/slurm.conf


