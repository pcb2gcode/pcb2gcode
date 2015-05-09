define(`m4_unit',3)dnl In micrometers, so 3 figures on the left regarding millimeters
define(`m4_resolution',`m4_addterm(*126)m4_addpow(-3)')dnl resolution in micrometerSPECIFIC FOR LPKF C60 PLOTTER
define(`m4_inv_resolution',`m4_addterm(*79375)m4_addpow(-4)')dnl resolution in micrometerSPECIFIC FOR LPKF C60 PLOTTER
define(`m4_header',`WD $1;')dnl
define(`recurse_strip0',`ifelse(len($1),`0',,ifelse(substr($1,0,1),`0',`recurse_strip0(`substr($1,1)')',`$1'))')dnl
define(`m4_strip0',`ifelse(substr($1,0,1),`-',`-'recurse_strip0(substr($1,1)),recurse_strip0($1))')dnl
define(`m4_stripdot',`define(`dotpos',`index($1,`.')')m4_strip0(substr($1,0,eval(dotpos))substr($1,eval(dotpos+1)))')dnl
define(`m4_calc_init',`define(`m4_term',`')define(`m4_pow',`')')dnl
define(`m4_addterm',`define(`m4_term',m4_term`'$1)')dnl
define(`m4_addpow',`define(`m4_pow',m4_pow`'$1)')dnl
define(`m4_result',`define(`m4_mantisse',eval(m4_term))define(`m4_exposant',eval(m4_pow))m4_expand(m4_mantisse,m4_exposant)')dnl
define(`m4_expand',`ifelse($2,`0',$1,ifelse(substr($2,0,1),`-',substr($1,0,eval(len($1)$2)),$1`recurse_pow0($2)'))')dnl
define(`recurse_pow0',`ifelse($1,`0',,0`recurse_pow0(eval($1-1))')')dnl
define(`m4_format',`
ifdef(`m4_imperial',`m4_calc_init`'m4_addterm(254)m4_addpow(-1)m4_addterm(*m4_stripdot($1))m4_addpow(-m4_dotdigit)m4_resolution`'m4_addpow(+3)m4_result')
ifdef(`m4_metric',`(m4_stripdot($1)*m4_resolution)')
')dnl
define(`m4_xcentre',`m4_calc_init`'m4_addterm(50/2)m4_addpow(4)m4_resolution`'m4_result')dnl
define(`m4_ycentre',`m4_calc_init`'m4_addterm(24/2)m4_addpow(4)m4_resolution`'m4_result')dnl
define(`m4_xoffset',`eval(m4_xcentre-(m4_format(m4_xmax)-1*m4_format(m4_xmin))/2)')dnl
define(`m4_yoffset',`eval(m4_ycentre-(m4_format(m4_ymax)-1*m4_format(m4_ymin))/2)')dnl
define(`m4_XY',`eval(m4_format(X)+m4_xoffset`'),eval(m4_format(Y)+m4_yoffset`')')dnl
dnl
dnl PREAMABLE / POSTAMBLE
dnl
define(`m4_preamble',`dnl
ifdef(`m4_noautoleveller',`m4_header(`Software-independent Gcode')')
ifdef(`m4_imperial',`dnl
')
ifdef(`m4_metric',`dnl
')
IN;
dnl !RS10;#um/step
dnl VS`'m4_feedrate;#um/s
dnl !VU;
dnl !AS1;
dnl !TS1;
!CM1
!CT1
!EM1
!FP
')dnl
define(`m4_postamble',`
')dnl
dnl
dnl Movement funtions
dnl
define(`m4_move',`define(`X',$1)define(`Y',$2)dnl
PU
PA`'m4_XY
PD')dnl
define(`m4_mill',`define(`X',$1)define(`Y',$2)dnl
PA`'m4_XY ')dnl
