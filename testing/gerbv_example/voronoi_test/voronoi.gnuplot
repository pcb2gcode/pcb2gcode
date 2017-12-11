rgb(r,g,b) = 65536 * (int(r)%256) + 256 * (int(g)%256) + (int(b)%256)
plot "file.dat" using 1:2:($3-$1):($4-$2):(rgb($1,$2,$3)) with vectors filled head lw 3 lt rgb variable
pause -1
