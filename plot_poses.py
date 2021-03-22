
####################################################################################################################
# Plot output deepDSO camera poses to PLY file for inspection
#
#       USAGE:
#
#       python3  plot_poses.py /home/ubuntu/my_deepdso_outputs/camera_poses.txt /home/ubuntu/my_deepdso_outputs/camera_poses.ply 
#
# camera_poses.txt is the output file produced from deepDSO. It has the format:
#   timestamp, file_prefix, T[0], T[1], T[2], R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7], R[8]
#
# Copyright 2021, Taylor Dahlke
####################################################################################################################


import sys
import numpy as np
from mpl_toolkits.mplot3d import axes3d
import matplotlib.pyplot as plt
from plyfile import PlyData, PlyElement

if __name__ == '__main__':

    print("Starting ...")
    poses_file = sys.argv[1]
    output_file = sys.argv[2]

    pos = []
    ply_vec = []
    numposes = 0
    print("Reading camera pose file ...")

    with open(poses_file) as f:
        for line in f:
            numposes = numposes + 1
            if (numposes == 1):
                continue  # Skip the first line which is a header
            a = np.fromstring(line, dtype=float, sep=' ')

            # Translation matrix (T)
            translationM = np.zeros((3, 1), dtype=float)
            translationM[0:3, 0] = np.transpose(a[2:5])

            # Rotation matrix (R)
            rotationM = np.zeros((3, 3), dtype=float)
            rotationM[0, 0:3] = a[5:8]
            rotationM[1, 0:3] = a[8:11]
            rotationM[2, 0:3] = a[11:14]

            # Calculate position
            rotationM_T = np.transpose(rotationM)*(-1.0)
            positionM = np.matmul(rotationM_T, translationM)
            pos.append(positionM)

            # Calculate the view vector
            rotationM_T = np.transpose(rotationM)
            unit_z_vec = np.array([0, 0, 1])
            view_vector = np.matmul(rotationM_T, unit_z_vec)

            # PLY vector file
            ply_vec.append((positionM[0], positionM[1], positionM[2],
                            view_vector[0], view_vector[1], view_vector[2]))

    print("Writing PLY file ...")
    TT = np.array(ply_vec, dtype=[('x', 'f4'), ('y', 'f4'),
                                  ('z', 'f4'), ('nx', 'f4'), ('ny', 'f4'), ('nz', 'f4')])
    el = PlyElement.describe(TT, 'vertex')
    PlyData([el], text=True).write(output_file)

    print("DONE!")
