# Lattice Agreement algorithms implementations

This repository contains implementation of four LA algorithms.

| `Algorithm name`                                                                                                                          |
|:------------------------------------------------------------------------------------------------------------------------------------------|
| [Zheng](https://drops.dagstuhl.de/opus/volltexte/2020/11815/pdf/LIPIcs-OPODIS-2019-29.pdf)                                                                                                                                 |
| [Faleiro one-shot](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/Generalized20Lattice20Agreement20-20PODC12.pdf)    |
| [Faleiro generalized](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/Generalized20Lattice20Agreement20-20PODC12.pdf) |
| Generalizer                                                                                                                               |

## Setup

1. git submodule init
2. git submodule update

## Build

3. `mkdir build`
4. `cd build`
5. `cmake ..`
6. `make`

## Run local test

1. Run coordinator
`coordinator <number of processes> <number of failures> 8000`
2. Run algorithm instances
`bash run_zheng.sh <number of processes> <process ip address> <coordinator ip  address>`

# License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/deffrian/lattice-agreement/LICENSE)

