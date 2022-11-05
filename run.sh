if [ $# -eq 0 ]; then
  dsmexec /tmp/machine_file exemple;
else
  if [ $1 = "debug" ]; then
    rm Phase1/bin/dsmexec Phase1/bin/dsmwrap;
    cp Original-bins/dsmexec-debug Phase1/bin/dsmexec
    cp Original-bins/dsmwrap-debug Phase1/bin/dsmwrap
    chmod u+x Phase1/bin/dsm*
  elif [ $1 = "silent" ]; then
    rm Phase1/bin/dsmexec Phase1/bin/dsmwrap;
    cp Original-bins/dsmexec Phase1/bin/dsmexec
    cp Original-bins/dsmwrap Phase1/bin/dsmwrap
    chmod u+x Phase1/bin/dsm*
  elif [ $1 = "perso" ]; then
    cd Phase1;
    make;
  elif [ $1 = "make" ]; then
    cd Phase2;
    make $2;
  else
    echo
    echo "Utilisation:"
    echo "            ./run.sh        : lance le programme"
    echo "            ./run.sh debug  : met en place le lanceur avec les debugs"
    echo "            ./run.sh silent : met en place le lanceur sans les debugs"
    echo "            ./run.sh perso  : met en place le lanceur de la phase 1"
    echo "            ./run.sh make   : lance le make de la Phase2"
    echo
    echo
  fi
fi
