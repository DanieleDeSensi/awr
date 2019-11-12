Introduction
------------
This repository contains the code of the library to automatically tune the adaptive routing algorithm on Cray Aries networks, presented in the paper "Mitigating Network Noise on Dragonfly Networks through Application-Aware Routing" by Daniele De Sensi, Salvatore Di Girolamo and Torsten Hoefler, presented at the 2019 International Conference for High Performance Computing, Networking, Storage, and Analysis.

Compiling
---------
To compile it, just run the "compile_daint.sh" script if you are using the Piz Daint machine or "compile_cori.sh" script if you are using the Cori machine.
If you are using a different system based on Cray Aries interconnection network, modify one of the two scripts provided.

Using
-----
AWR can be used in a "fixed mode", where the user manually selects the routing algorithm and the algorithm described in Section 4. is not executed, or in an "automatic mode", where the best routing algorithm is automatically selected by the library.

Fixed Mode
==========
To manually change the routing algorithm, include the "awr.h" header in your application and link it to libawr. Then, you can change the routing algorithm before performing a network communication by calling the 'awr_change_routing' function. The parameter of this function will be the identifier of the routing algorithm as defined in the "gni_pub.h" header. 

Automatic mode
==============
To use AWR in automatic mode, simply specify:
$ export LD_PRELOAD=${CURRENT}/build/libawr.so

Where ${CURRENT} is the absolute path of the folder where this README file is located.

NOTE: To properly load libawr, before compiling your application you should:
$ export CRAYPE_LINK_TYPE="dynamic"

Publication
-----------

If you use awr, cite us:
```bibtex
@inproceedings{awr,
  author    = {De Sensi, Daniele and Di Girolamo, Salvatore and Hoefler, Torsten},
  title     = {Mitigating Network Noise on Dragonfly Networks through Application-Aware Routing},
  year      = {2019},
  booktitle = {Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis},
  series = {SC '19}
}
```