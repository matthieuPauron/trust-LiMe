@page TrustLiMe_intro Introduction

## A TRUST-based application

TRUST-LiMe is a C++ module built on the TRUST kernel
([TRUST paper](https://doi.org/10.1051/epjn/2025050)). The TRUST kernel is
developed at CEA, more precisely in DES/ISAS/DM2S/SGLS/LCAN, and its C++ source
code is hosted [on GitHub](https://github.com/cea-trust-platform). Its
[documentation website](https://cea-trust-platform.readthedocs.io/en/latest/)
contains a lot of information about the capabilities of TRUST, as well as
several tutorials for the interested user. If you are interested in using
TRUST-LiMe but do not have hands-on experience with the TRUST code, it is
strongly recommended to work through the
[user tutorials](https://cea-trust-platform.readthedocs.io/en/latest/user_tutorials/index.html)
first. A short and incomplete list of the characteristics of TRUST:

- massive parallelisation of computations;
- GPU porting of the source code (ongoing as of December 2025);
- *(to complete)*;
- compatibility with daughter applications such as TrioCFD for turbulence
  models, [TRUST-NK](https://doi.org/10.1051/epjconf/202430203007) for
  deterministic core neutronics, and others.

TRUST builds a single executable from the kernel code and the sources of any
number of modules based on that kernel. This yields an executable that is
natively able to perform multi-physics computations: for example, a
TRUST + TRUST-NK + TrioCFD executable can perform coupled neutronics and
turbulent thermalhydraulics computations. It is this modularity of the TRUST
kernel that motivated the creation of TRUST-LiMe.

## The goal of TRUST-LiMe

The goal of TRUST-LiMe is to build software able to natively perform coupled
turbulent MHD, and eventually MHD coupled with neutronics. The target
application is the study of breeder layers in stellarator reactors.

## Physical limitations

**To be completed.** Indicate what kind of plasma can be simulated with the code (how dilute, how strongly ionised).
