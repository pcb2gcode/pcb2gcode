define(`m4_header',`( $1 )')dnl
define(`m4_preamble',`dnl
ifdef(`m4_noautoleveller',`( Software-independent Gcode )')
      
ifdef(`m4_imperial',`dnl
G94 ( Inches per minute feed rate. )
G20 ( Units == INCHES. )
')
G90 ( Absolute coordinates. )
S`'m4_spindlespeed ( RPM spindle speed. )
G64 P`'m4_maxdeviation ( set maximum deviation from commanded toolpath )
F`'m4_feedrate ( Feedrate. )

F`'m4_feedrate ( Feedrate. )
M3 ( Spindle on clockwise. )')dnl
define(`m4_move',`define(`X',$1)define(`Y',$2)dnl
G04 P0 ( dwell for no time -- G64 should not smooth over this point )
G00 Z`'m4_zsafe ( retract )

G00 `X'X` Y'Y ( rapid move to begin. )
G01 Z`'m4_zwork
G04 P0 ( dwell for no time -- G64 should not smooth over this point )')dnl
define(`m4_mill',`define(`X',$1)define(`Y',$2)`X'X `Y'Y')dnl
define(`m4_postamble',`
G04 P0 ( dwell for no time -- G64 should not smooth over this point )
G00 Z1.00000 ( retract )

M5 ( Spindle off. )
M9 ( Coolant off. )
M2 ( Program end. )
')dnl
