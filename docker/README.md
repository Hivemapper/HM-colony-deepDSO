# How to build deepDSO using Docker

## Requirements
- A linux based docker host machine with at least one NVIDIA GPU<sup>[1](#f1)</sup>.

## Quick Start:

1. Check that Docker >=19.03<sup>[2](#f2)</sup> installed on your docker host machine:
`docker --version`

If not, then install docker. For Centos 7/8 this means:
`yum install -y docker`
`sudo systemctl start docker`

For Debian machines this means:
`sudo apt install docker.io`

2. Check that you have an NVIDIA driver installed on your docker host machine:
`nvidia-smi`

If not, then read [these instructions](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/install-nvidia-driver.html#preinstalled-nvidia-driver) for a G3 machine.

3. Setup the nvidia-toolkit on your host machine<sup>[3](#f3)</sup>:

For Ubuntu host machines: `./setup-nvidia-docker-toolkit.sh `

For CentOS host machines: `./setup-nvidia-docker-toolkit.sh centos`

4. Run the *quick-start* script, using the *full path* to your prefered working directory on the docker host machine (a folder with your input files/images, etc.):

`./quick-start.sh /path/where/your/working/folder/is`

If the docker host machine is remote (like an AWS instance that you've SSHed into), then run this instead:

`./quick-start.sh /path/where/your/working/folder/is remote`

Note, running on a remote machine and having the GUI forwarded over X11 is still under development as of 4/23/21.
 
5. Now you should be in a directory (inside the Docker container) mounted to the
working folder path you specified. You will want to have a `.pt` DNN model available for the inference part of deepDSO to use. You can find several pre-trained DNN's [here](https://github.com/TRI-ML/packnet-sfm#models). Once you've downloaded one of these, you'll need to convert it to a PyTorch (`.pt`) file:

`python3 convert_model_to_jit.py --checkpoint [path/to/inputDNN.ckpt] --input [path/to/example_image.png] --output [path/to/outputDNN.pt]`

Where `example_image.png` is an example image that has the dimensions expected by the `inputDNN.ckpt` file.

6. Now you can run the deepDSO binary on your own inputs like this:

` ./build/bin/dso_dataset \
		files=../../dataset_color/sequences/00/0-200/ \
		outputs=../outputs \
		calib=./camera.txt \
		cnn=../PackNet01_HR_velsup_CStoK_jit.pt \
		preset=0 \
		mode=1 \
		nogui=1`

The `run_dso.sh` script can be useful for this as well.

## Build from Scratch

After completing steps 1-3, you can alternatively build the docker image from
scratch based on the **Dockerfile** (e.g., with your own modifications) using:

`./build_docker.sh`

## Notes

Running deepDSO binaries can use a lot of memory (depending on the size of your
data set / imagery). Docker has a relatively small default memory setting
(2Gb on Mac). You will probably want to increase this before you run any larger
workflows. From Docker desktop on Mac for example, just open the Docker GUI, go
to the *Advanced* tab and increase via the slider:

![](docker-memory-settings.png?raw=true)

<a name="f1">1</a>: deepDSO needs NVIDA GPU compute hardware (as of 1/27/2021).

<a name="f2">2</a>: This is because Docker 19.03+ natively supports NVIDIA GPUs.

<a name="f3">3</a>: You should get a similar output to what you get when you ran step 2 on your host, since the docker container is detecting the same GPU(s). If you have trouble, you may want to read the [nvidia-docker](https://github.com/NVIDIA/nvidia-docker) webpage, as the scripts `./setup-ubuntu.sh` and `./setup-centos.sh` are based on instructions posted there and may change over time.
