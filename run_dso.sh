  #!/usr/bin/env bash  
 
 ./build/bin/dso_dataset \
		files=../dataset_gray/sequences/00/image_0 \
		calib=./camera.txt \
		cnn=../PackNet01_HR_velsup_CStoK_jit.pt \
		preset=3 \
		mode=2 \
		nogui=1