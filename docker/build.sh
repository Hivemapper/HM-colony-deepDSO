docker build -t="deepdso:centos8" .;
docker run -w /working -v $1:/working:z -it deepdso:centos8;