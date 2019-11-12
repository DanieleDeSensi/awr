Introduction
------------
This repository contains the code of the library to automatically tune the adaptive routing algorithm on *Cray Aries* networks, presented in the paper ***"Mitigating Network Noise on Dragonfly Networks through Application-Aware Routing"***, by Daniele De Sensi, Salvatore Di Girolamo and Torsten Hoefler, presented at the *2019 International Conference for High Performance Computing, Networking, Storage, and Analysis (SC)*.

Compiling
---------
To compile it, just run the *compile_daint.sh* script if you are using the *Piz Daint* system or *compile_cori.sh* script if you are using the *Cori* system.
If you are using a different system based on *Cray Aries* interconnection network, modify one of the two scripts provided.

Using
-----
AWR can be used in a *"fixed mode"*, where the user manually selects the routing algorithm, or in an *"automatic mode"*, where the best routing algorithm is automatically selected by the library (by using the algorithm described in *Section 4* of the paper).

Fixed Mode
==========
To manually change the routing algorithm, include the `awr.h` header in your application and link it to *libawr*. Then, you can change the routing algorithm before performing a network communication by calling the `awr_change_routing` function. The parameter of this function will be the identifier of the routing algorithm. It can be one of the following values:

- **GNI_DLVMODE_ADAPTIVE0**: This mode is configured as adaptive, no-bias routing.
Aries supports four adaptive routing control modes (**GNI_DLVMODE_ADAPTIVE0-3**), each of which select a different bias between minimal and non-minimal routes. Minimal paths are shorter, generate a lower load on the network, and are best when traffic is uniformly distributed. Non-minimal routes have more router-to-router hops, generate a greater load on the network, and are better when the minimal route would result in a load imbalance. These adaptive routing control modes do not change the routes, only the proportions of minimal and non-minimal routes. Aries provides packet-by-packet adaptive routing. The Aries system updates the load estimates so that each of the adaptive routing behaviors may change according to network load.
- **GNI_DLVMODE_ADAPTIVE1**: This mode is configured as adaptive, increasing minimal bias.
- **GNI_DLVMODE_ADAPTIVE2**: Adaptive, small bias against non-minimal paths.
- **GNI_DLVMODE_ADAPTIVE3**: Adaptive, large bias against non-minimal paths.
- **GNI_DLVMODE_MIN_HASH**: Routing is minimal, hashed, non-adaptive. The route used to reach the destination is chosen from the set of routes that are the most direct (use the least number of hops). For a given source and destination endpoint pair, strict ordering is maintained-only amongst packets targeting the same destination memory cacheline.
- **GNI_DLVMODE_NMIN_HASH**: Routing is non-minimal, hashed, non-adaptive. The route used to reach destination may not be the most direct. For a given source and destination endpoint pair, strict ordering is maintained only amongst packets targeting the same destination memory cacheline.
- **GNI_DLVMODE_IN_ORDER**: Routing is minimal, non-hashed, non-adaptive. For a given source and destination endpoint pair, strict ordering is maintained amongst all packets issued with this same delivery mode.



Automatic mode
==============
To use AWR in automatic mode, simply specify:
```bash
$ export LD_PRELOAD=${CURRENT}/build/libawr.so
```

Where `${CURRENT}` is the absolute path of the folder where this README file is located.

**NOTE**: To properly load libawr, before compiling your application you should:
```bash
$ export CRAYPE_LINK_TYPE="dynamic"
```

Publication
-----------

If you use awr, please cite our paper:
```bibtex
@inproceedings{DeSensi:2019:MNN:3295500.3356196,
    author = {De Sensi, Daniele and Di Girolamo, Salvatore and Hoefler, Torsten},
    title = {Mitigating Network Noise on Dragonfly Networks Through Application-aware Routing},
    booktitle = {Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis},
    series = {SC '19},
    year = {2019},
    isbn = {978-1-4503-6229-0},
    location = {Denver, Colorado},
    pages = {16:1--16:32},
    articleno = {16},
    numpages = {32},
    url = {http://doi.acm.org/10.1145/3295500.3356196},
    doi = {10.1145/3295500.3356196},
    acmid = {3356196},
    publisher = {ACM},
    address = {New York, NY, USA},
    keywords = {dragonfly, network noise, routing},
} 
```