define(`m4_header',`G04 $1 *')dnl
define(`m4_preamble',`G04 File Generated using pb2gcode and m4 template gerber.m4*
`%FSLAX'm4_digitdot`'m4_dotdigit`Y'm4_digitdot`'m4_dotdigit`*%'
ifdef(`m4_metric',`%MOMM*%')`'dnl
ifdef(`m4_imperial',`%MOIN*%')
%TF.Part,Other*%
%LPD%
%ADD10C,0.010*%
D10*
G01*')dnl
define(`m4_coord',`define(`dotpos',`index($1,`.')')substr($1,0,eval(dotpos))substr($1,eval(dotpos+1))')dnl
define(m4_postamble,M02*)dnl
define(m4_header,)dnl
define(m4_move,`define(`X',$1)define(`Y',$2)`X'm4_coord(X)`Y'm4_coord(Y)`D02*'')dnl
define(m4_mill,`define(`X',$1)define(`Y',$2)`X'm4_coord(X)`Y'm4_coord(Y)`D01*'')dnl
define(plunge,`G04 Plunge')dnl
define(m4_Z,`G04 Z:$1')dnl
