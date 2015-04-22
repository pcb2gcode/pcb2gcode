PCB2GCODE_BASE=/home/saintel/pcb2gcode/git_20150418/pcb2gcode/
PCB2GCODE=$PCB2GCODE_BASE/pcb2gcode
PCB2GCODE_M4=$PCB2GCODE_BASE/pcb2gcode_m4
MILLPROJECT=millproject

if [ -a $MILLPROJECT ] ; then {
   $PCB2GCODE --m4=true
   

   if (grep -q front $MILLPROJECT); then {

      if (grep -q ^front-output $MILLPROJECT); then {
         FRONT_OUTPUT=`grep front-output millproject | sed -e 's/^[ ]*front-output[ ]*=[ ]*\([^ \t]\+\)/\1/'`
      }; else { 
         FRONT_OUTPUT='front' 
      }; fi;
      FRONT_OUTPUT_M4=$FRONT_OUTPUT.m4
      FRONT_OUTPUT_GBR=$FRONT_OUTPUT.gbr

      if [ -a $FRONT_OUTPUT_M4 ]; then {
         echo "Processing front file : $FRONT_FILE" 
         m4 $PCB2GCODE_M4/gerber.m4 $FRONT_OUTPUT_M4 > $FRONT_OUTPUT_GBR
      }; else {
         echo "Front template file not found : $FRONT_OUTPUT_M4" 
      }; fi;

   } fi;
   
   if (grep -q ^back $MILLPROJECT); then {

      if (grep -q back-output $MILLPROJECT); then {
         BACK_OUTPUT=`grep back-output millproject | sed -e 's/^[ ]*back-output[ ]*=[ ]*\([^ \t]\+\)/\1/'`
      }; else { 
         BACK_OUTPUT='back' 
      }; fi;
      BACK_OUTPUT_M4=$BACK_OUTPUT.m4
      BACK_OUTPUT_GBR=$BACK_OUTPUT.gbr

      if [ -a $BACK_OUTPUT_M4 ]; then {
         echo "Processing back file : $BACK_FILE" 
         m4 $PCB2GCODE_M4/gerber.m4 $BACK_OUTPUT_M4 > $BACK_OUTPUT_GBR
      }; else {
         echo "Back template file not found : $BACK_OUTPUT_M4" 
      }; fi;

   } fi;

   if (grep -q outline $MILLPROJECT); then {

      if (grep -q outline-output $MILLPROJECT); then {
         OUTLINE_OUTPUT=`grep outline-output millproject | sed -e 's/^[ ]*outline-output[ ]*=[ ]*\([^ \t]\+\)/\1/'`
      }; else { 
         OUTLINE_OUTPUT='outline' 
      }; fi;
      OUTLINE_OUTPUT_M4=$OUTLINE_OUTPUT.m4
      OUTLINE_OUTPUT_GBR=$OUTLINE_OUTPUT.gbr

      if [ -a $OUTLINE_OUTPUT_M4 ]; then {
         echo "Processing outline file : $OUTLINE_FILE" 
         m4 $PCB2GCODE_M4/gerber.m4 $OUTLINE_OUTPUT_M4 > $OUTLINE_OUTPUT_GBR
      }; else {
         echo "Outline template file not found : $OUTLINE_FILE_M4" 
      }; fi;

   } fi;
   
} fi;

