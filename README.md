# polyoptim

Optimal Trajectory Generation in Table Tennis

This repository explores two different optimal control strategies in table tennis and compare
them to an existing Virtual Hitting Plane (VHP) based method. 

We use nonlinear (in)equality constrained optimization
to generate table tennis striking (and returning) 3rd order
polynomials in joint space. 

We use the NLOPT toolbox to run the constrained nonlinear optimizations:
http://ab-initio.mit.edu/wiki/index.php/NLopt
