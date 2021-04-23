"""
this script is used for converting your pytorch model into torchscript that could be used under CPP environment. 
The following code is extended from  ``` scripts/evaluate_depth.py ``` of  [packnet-sfm](https://github.com/TRI-ML/packnet-sfm),
"""
import argparse
import numpy as np
import os
import torch

from torch.utils.data import DataLoader
from tqdm import tqdm

# from train_sfm_utils import load_dispnet_with_args

from packnet_sfm.models.model_wrapper import ModelWrapper
from packnet_sfm.datasets.augmentations import resize_image, to_tensor
from packnet_sfm.utils.horovod import hvd_init, rank, world_size, print0
from packnet_sfm.utils.image import load_image
from packnet_sfm.utils.config import parse_test_file
from packnet_sfm.utils.load import set_debug
from packnet_sfm.utils.depth import write_depth, inv2depth, viz_inv_depth
from packnet_sfm.utils.logging import pcolor
from packnet_sfm.geometry.pose import Pose

import pdb

def is_image(file, ext=('.png', '.jpg',)):
    """Check if a file is an image with certain extensions"""
    return file.endswith(ext)


def parse_args():
    parser = argparse.ArgumentParser(description='Convert PyTorch Packnet Model to Torchscript')
    parser.add_argument('--checkpoint', type=str, help='Checkpoint (.ckpt)')
    parser.add_argument('--input', type=str, help='Example image file')
    parser.add_argument('--output', type=str, help='Output path for JIT model (.pt)')
    parser.add_argument('--image_shape', type=int, nargs='+', default=None,
                        help='Input and output image shape '
                             '(default: checkpoint\'s config.datasets.augmentation.image_shape)')
    parser.add_argument('--half', action="store_true", help='Use half precision (fp16)')
    args = parser.parse_args()
    assert args.checkpoint.endswith('.ckpt'), \
        'You need to provide a .ckpt file as checkpoint'
    assert args.output.endswith('.pt'), \
        'You need to provide a .pt file as output'
    assert args.image_shape is None or len(args.image_shape) == 2, \
        'You need to provide a 2-dimensional tuple as shape (H,W)'
    return args


@torch.no_grad()
def convert(model_wrapper, input_file, output_file, image_shape, half):
    """
    Process a single input file to produce and save visualization

    Parameters
    ----------
    input_file : str
        Image file
    output_file : str
        Output file, or folder where the output will be saved
    model_wrapper : nn.Module
        Model wrapper used for inference
    image_shape : Image shape
        Input image shape
    half: bool
        use half precision (fp16)
    save: str
        Save format (npz or png)
    """
    # change to half precision for evaluation if requested
    dtype = torch.float16 if half else None

    image = load_image(input_file)
    # Resize and to tensor
    image = resize_image(image, image_shape)
    image = to_tensor(image).unsqueeze(0)
    # Send image to GPU if available
    if torch.cuda.is_available():
        image = image.to('cuda:{}'.format(rank()), dtype=dtype)

    
    try:
        model_wrapper.depth_net.tracing = True
    except:
        pass

    traced_script_module = torch.jit.trace(model_wrapper.depth_net, image, check_trace= False)#convert
    # traced_script_module = torch.jit.trace_module(model_wrapper.depth_net, {'trace_forward': image}, check_trace= False)#convert
    # save torchscript model
    traced_script_module.save(output_file)


def main(args):

    # Initialize horovod
    hvd_init()

    # Parse arguments
    config, state_dict = parse_test_file(args.checkpoint)
    # pdb.set_trace()

    # If no image shape is provided, use the checkpoint one
    image_shape = args.image_shape
    if image_shape is None:
        image_shape = config.datasets.augmentation.image_shape

    # Set debug if requested
    set_debug(config.debug)

    # Initialize model wrapper from checkpoint arguments
    model_wrapper = ModelWrapper(config, load_datasets=False)
    # Restore monodepth_model state
    model_wrapper.load_state_dict(state_dict)

    # change to half precision for evaluation if requested
    dtype = torch.float16 if args.half else None

    # Send model to GPU if available
    if torch.cuda.is_available():
        model_wrapper = model_wrapper.to('cuda:{}'.format(rank()), dtype=dtype)

    # Set to eval mode
    model_wrapper.eval()

    # Process each file
    # for fn in files[rank()::world_size()]:
    convert(model_wrapper, args.input, args.output, image_shape, args.half)

if __name__ == '__main__':
    args = parse_args()
    main(args)