                                             # Global performance file #

This is the global file for tracking performance in TRUST. It stores aggregated quantities.
More detailed statistics can be found in the /home/matthieu/Development/trust-LiMe/share/Validation/Rapports_automatiques/MagneticStandalone/src/vector_components_csv.TU file
For time loop, only standard counters of level 1 are printed alongside your custom counters
Time is given in seconds

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                                              Context of the computation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Date:     11-10-2026 -- 22:50:58
OS:       matthieu-precisiont5610__Linux__x86_64__7.0.0-30-generic__#30~24.04.1-Ubuntu SMP PREEMPT_DYNAMIC Fri Aug  7 13:27:52 U
CPU model : AMD Ryzen 5 5600X 6-Core Processor
Total number of threads:12
GPU model: No GPU used for the computation
Nb procs used for the computation: 1
TRUST version: 1.9.8
Total number of elements used for the calculation: 2304

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                                           Computation start-up statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Total time of the start-up:                                                0.0193433      
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                                                 Time loop statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Total time of the time loop:                                               0.135101       
Number of time steps:                                                      100            
Skipped time steps:                                                        0              
Average time per time step:                                                0.00135101     
Standard deviation between time steps:                                     0.00700414     
Time elapsed in the skipped time steps:                                    0              


Standard counter description             | Time/step       | % loop time | Call(s)/step   
------------------------------------------------------------------------------------------
Linear solver resolutions Ax=B           | 0.000385541     | 28.5        | 1              
Call to ::ajouter_blocs for matrix assembly | 0.0005456449    | 40.4        | 1              
Source terms                             | 4.757e-08       |  0.0        | 1              
Update ::mettre_a_jour                   | 1.905904e-05    |  1.4        | 1              
Computation of the time step dt          | 0.0001293668    |  9.6        | 2              
Post-treatment operations                | 1.298391e-05    |  1.0        | 1              
Other operations                         | 0.0002583634    | 19.1        | 

Average number of iteration of the linear solver per call:                 6              

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                                              Post-resolution statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Time of the post-resolution:                                               0.000212102    

Total time for the whole computation                                       0.154693       

