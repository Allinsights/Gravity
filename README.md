

# ARMO

## Supported platforms

ARMO was only tested on Mac OS and Linux, no Windows support at this point.

## Dependencies

ARMO's only dependency is [Cmake](https://cmake.org/) 3.2 or higher and a C++ compiler.


## Compiling


ARMO will download and link to [Ipopt](https://github.com/coin-or/Ipopt) and [LASlib](https://github.com/LAStools/LAStools).

For a faster Ipopt, preferably build it with HSL libraries and make sure to specify the path to `IPOPT_ROOT_DIR` in your `bash_profile` file, e.g.:

`export IPOPT_ROOT_DIR="/Users/yourname/Dev/CoinIpopt/build"`

To run the MIQCPs with [Gurobi](http://www.gurobi.com), please install it on your system first.

Then enter:
`mkdir build`
`cd build`
`cmake -DGurobi=ON ..` if Gurobi is installed in your system otherwise just enter:
`cmake ..`

Finally enter:
`make -j4`

## Running

For Registration:

`Gravity/bin/Release/ARMO Reg /path_to_toy_model.txt /path_to_toy_data.txt ARMO global` (This will run the nonconvex MIQCP model)

or 

`Gravity/bin/Release/ARMO Reg /path_to_toy_model.txt /path_to_toy_data.txt ARMO global convex` (This will run the convex MIQCP relaxation model)

or

`Gravity/bin/Release/ARMO Reg /path_to_toy_model.txt /path_to_toy_data.txt GoICP` (This will run the Go-ICP algorithm)

For Boresight Alignment:

`Gravity/bin/Release/ARMO Align /path_to_Cars_model.txt /path_to_Cars_data.txt /path_to_Cars_model_sub.txt /path_to_Cars_data_sub.txt`

The first two command line arguments should point to the full point clouds, the second two should point to the subsampled point clouds.

Datasets can be downloaded [here](https://c6cff554-9579-44a7-959e-fab75fd5d22a.usrfiles.com/archives/c6cff5_402c21969b5d4bc49a340f97607027b1.zip) (full point clouds) and [here](https://c6cff554-9579-44a7-959e-fab75fd5d22a.usrfiles.com/archives/c6cff5_e271c09cc9824d0686aed597678615ec.zip) (subsampled point clouds)

## Go-ICP
ARMO includes the source code of [Go-ICP](https://github.com/yangjiaolong/Go-ICP) for comparison purposes.
