Build Slurm Docker
==================


```
sudo docker build slurm_docker
sudo docker run -v /sys/fs/cgroup:/sys/fs/cgroup:rw -it --hostname slurm0 819045e3d915

./start_slurm.sh

srun --pty bash
```

