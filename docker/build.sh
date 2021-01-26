docker build -t="deepdso:hivemapper" .;
docker run -w /working -v $1:/working:z -it deepdso:hivemapper;