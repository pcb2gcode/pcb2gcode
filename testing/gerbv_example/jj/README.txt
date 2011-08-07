This is an example from Joachim Jansen (EKF Elektronik GmbH Industrial 
Computers & Information Technology) who kindly sent this files to 
point out some bugs in gerbv. When I (spe) had gone through them I had
found at least four bugs.

The files are:
 l1-orig.grb : The original file. Contains filled polygons with circles as
               outline and three layers.
 l1-?.grb:     The three layers in separate files. Layer 1 is drawn, layer 2 
               is "carved out" from layer 1 and layer3 is then finally the 
	       top layer.

$Id$