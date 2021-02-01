####################################################################
#
#   USAGE:  
#       1)  If docker host is your local machine:
#               ./quick_start.sh ~/my_testing_dir 
#
#       2)  If docker host is a remote machine (like an AWS instance):
#                ./quick_start.sh ~/my_testing_dir remote
#
####################################################################

if [ $2 = 'remote' ]; then
    echo "Setting up for docker host on a remote machine"
    # These steps are needed to forward the GUI functionality from the docker container ---> remote docker host --> your local display
    XSOCK=/tmp/.X11-unix
    XAUTH=/tmp/.docker.xauth
    xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | sudo xauth -f $XAUTH nmerge -
    sudo chmod 777 $XAUTH
    X11PORT=`echo $DISPLAY | sed 's/^[^:]*:\([^\.]\+\).*/\1/'`
    TCPPORT=`expr 6000 + $X11PORT`
    sudo ufw allow from 172.17.0.0/16 to any port $TCPPORT proto tcp 
    DISPLAY=`echo $DISPLAY | sed 's/^[^:]*\(.*\)/172.17.0.1\1/'`
    docker run --gpus all -w /working  --rm -e DISPLAY=$DISPLAY -v $1:/working -v $XAUTH:$XAUTH -e XAUTHORITY=$XAUTH -it deepdso/deepdso:centos8;
else
    # Pull & start docker container
    docker pull deepdso/deepdso:centos8 # Comment this line if you wish to use your own local build of deepdso/deepdso:centos8
    docker run --gpus all -w /working -v $1:/working -it deepdso/deepdso:centos8;
fi
