#!/bin/bash

if [ ! -d glucose-4 ]; then
    wget www.labri.fr/perso/lsimon/downloads/softwares/glucose-syrup-4.1.tgz
    tar xzvf glucose-syrup-4.1.tgz
    mv glucose-syrup-4.1 glucose-4
fi
make