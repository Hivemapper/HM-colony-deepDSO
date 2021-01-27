docker pull deepdso/deepdso:centos8
docker run --gpus all -w /working -v $1:/working -it deepdso/deepdso:centos8;
