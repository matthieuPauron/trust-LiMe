---
title: Introduction
---

# Introduction

## A TRUST-based application

TRUST-LiMe is a C++ module of the TRUST kernel [](https://doi.org/10.1051/epjn/2025050). The TRUST kernel is developped in CEA, more precisely in DES/ISAS/DM2S/SGLS/LCAN and the C++ source code is hosted [on github](https://github.com/cea-trust-platform). Their [documentation website](https://cea-trust-platform.readthedocs.io/en/latest/) contains loads of information about the capabilities of TRUST, as well as several tutorials for the interested user. If you are interested in using TRUST-LiMe but do not have a hands-on experience of the TRUST code, I strongly suggest that you check the [user tutorials](https://cea-trust-platform.readthedocs.io/en/latest/user_tutorials/index.html). A short and incomplete list of the characteristics of TRUST :
- massive parallelization of computations ;
- (to complete)
- GPU porting of the source code (ongoing as of dec. 2025) ;
- and compatibility with the daughter-applications such as TrioCFD for turbulence models, [TRUST-NK]( https://doi.org/10.1051/epjconf/202430203007 ) for deterministic core neutronics , and others... 

TRUST allows to build a single executable from the kernel code, and the source of any number of modules based on the TRUST kernel. This allows to build an executable that is natively able to perform multi-physics computations. For example, a TRUST+TRUST-NK+TrioCFD executable allows to perform coupled neutronics and turbulent thermalhydraulics computations. It is the modular aspect of the TRUST kernel that motivated the creation of TRUST-LiMe.

## The goal of TRUST-LiMe

The goal of TRUST-LiMe is to build a software that could natively perform coupled turbulent MHD coupled with neutronics. The application is the study of breeder layers in sellarator reactors.

## Physical limitations

**Indicate what kind of plasma can be simulated using the code (how diluted, how strongly ionized)**
