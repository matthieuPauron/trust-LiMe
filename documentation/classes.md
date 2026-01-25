# Problems, equations, boundary conditions

## Equations

### Convection diffusion formulation for the induction equation

$$
\partial_t b  - \nabla \times (u \times b) -  \Delta b = 0
$$

Assuming that the fluid is incompressible, we use the Gauss law to rewrite the above equation under the convection diffusion formulation

$$
\partial_t b - (u \cdot \nabla) b - \Delta b = (b \cdot \nabla) u
$$

It is implemented as a daughter class of the 

## Source terms

### Induction equation source term 

$(b \cdot \nabla ) u$ : not implemented yet

### Momentum equation source term : the Lorentz force

$(\nabla \times b) \times b)$ : not implemented yet

## Problems

The system of equations consists of four equations :
- the momentum equation ;
- the incompressibility constraint ;
- the induction equation ;
- the Gauss Law ;

We follow the 


