####################################################################
#
#   USAGE:  
#       1)  If docker host is debian based:
#               ./setup-nvidia-docker-toolkit.sh 
#
#       2)  If docker host is centos based:
#                ./setup-nvidia-docker-toolkit.sh centos
#
####################################################################

# Add the package repositories
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.repo | sudo tee /etc/yum.repos.d/nvidia-docker.repo

# Install nvidia-container-toolkit
if [ $1 = 'centos' ]; then
    sudo yum install -y nvidia-container-toolkit
    sudo systemctl restart docker
else
    sudo apt-get install -y nvidia-container-toolkit
    sudo systemctl restart docker
fi

# Check that it worked!
docker run --gpus all nvidia/cuda:10.2-base nvidia-smi
