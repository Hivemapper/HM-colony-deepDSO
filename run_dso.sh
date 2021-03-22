  #!/usr/bin/env bash  
 
 ./build/bin/dso_dataset \
		files=../dataset_color/sequences/00/test0-200 \
		outputs=../deepDSO_outputs \
		calib=./camera.txt \
		cnn=../PackNet01_HR_velsup_CStoK_jit.pt \
		preset=0 \
		mode=1 \
		nogui=1