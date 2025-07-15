#!/bin/bash

# Ustaw ścieżki do bibliotek
LIBDIR="/usr/lib/arm-linux-gnueabihf"
INCDIR="/usr/include"

# Kompiluj z bezpośrednim linkowaniem bibliotek
gcc linphone_p2p_custom_devices.c -o sip_receiver \
    -I${INCDIR} \
    -I${INCDIR}/linphone \
    -I${INCDIR}/mediastreamer2 \
    -I${INCDIR}/ortp \
    -L${LIBDIR} \
    -llinphone \
    -lmediastreamer \
    -lbctoolbox \
    -lortp \
    -lpthread \
    -ldl \
    -lm

if [ $? -eq 0 ]; then
    echo "Kompilacja zakończona sukcesem. Uruchom:"
    echo "sudo LD_LIBRARY_PATH=${LIBDIR} ./sip_receiver"
else
    echo "Błąd kompilacji!"
fi
