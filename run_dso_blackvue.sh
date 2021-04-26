  #!/usr/bin/env bash  
 
 ./build/bin/dso_dataset \
		files=../dataset_color/blackvue/New_Orleans/Michael/20201102_122459_NF/images \
		outputs=../deepDSO_outputs \
		calib=../dataset_color/blackvue/camera.txt \
		cnn=../blackvue-packnet-neworleans2-e36.pt \
		preset=0 \
		mode=1 \
		nogui=1 
